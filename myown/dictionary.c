#include "wordmate.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <errno.h>

static int make_dir(const char *path)
{
    if (mkdir(path, 0755) == 0 || errno == EEXIST)
        return 0;

    return -1;
}

#define MAKE_DIR(path) make_dir(path)
#endif


/* Write one record using the existing tab-separated format. */
static int write_entry(FILE *out, const Word *w)
{
    return fprintf(out, "%s\t%s\t%s\n",
                   w->word, w->meaning, w->synonyms) >= 0;
}


/* Split one line into word, meaning, and synonyms. */
static int parse_line(char *line, Word *w)
{
    char *first;
    char *second;

    first = strchr(line, '\t');

    if (first == NULL)
        return 0;

    *first = '\0';
    first++;

    second = strchr(first, '\t');

    if (second == NULL)
        return 0;

    *second = '\0';
    second++;

    second[strcspn(second, "\r\n")] = '\0';

    snprintf(w->word, sizeof(w->word), "%s", line);
    snprintf(w->meaning, sizeof(w->meaning), "%s", first);
    snprintf(w->synonyms, sizeof(w->synonyms), "%s", second);

    normalize_word(w->word);

    return 1;
}


/* Create the required folders and alphabet files. */
int ensure_data_ready(void)
{
    int i;
    char path[256];
    FILE *file;

    MAKE_DIR(DATA_DIR);
    MAKE_DIR(DICTIONARY_DIR);

    for (i = 0; i < 26; i++) {
        snprintf(path, sizeof(path),
                 DICTIONARY_DIR "/%c.txt", 'a' + i);

        file = fopen(path, "a");

        if (file == NULL) {
            perror(path);
            return 0;
        }

        fclose(file);
    }

    return 1;
}


/* Search for a word in its alphabetic file. */
int dictionary_search(const char *word)
{
    char target[WORD_MAX];
    char path[256];
    char line[LINE_MAX];
    FILE *file;
    Word entry;
    int found = 0;

    snprintf(target, sizeof(target), "%s",
             word ? word : "");

    normalize_word(target);

    if (!word_file_path(target, path, sizeof(path))) {
        puts("Invalid word.");
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        perror(path);
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char copy[LINE_MAX];

        snprintf(copy, sizeof(copy), "%s", line);

        if (parse_line(copy, &entry) &&
            strcmp(entry.word, target) == 0) {

            printf("\nWord: %s\n", entry.word);
            printf("Meaning: %s\n", entry.meaning);

            if (entry.synonyms[0] != '\0')
                printf("Synonyms: %s\n", entry.synonyms);

            found = 1;
        }
    }

    fclose(file);

    if (!found)
        printf("'%s' was not found.\n", target);

    return found;
}


/*
   Shared helper:
   mode 1 = add
   mode 2 = delete
   mode 3 = update
*/
static int rewrite_file(const char *path,
                        const char *target,
                        const Word *replacement,
                        int mode)
{
    char temp[300];
    char line[LINE_MAX];
    FILE *in;
    FILE *out;
    Word entry;
    int matches = 0;
    int ok = 1;

    snprintf(temp, sizeof(temp), "%s.tmp", path);

    in = fopen(path, "r");
    out = fopen(temp, "w");

    if (out == NULL) {
        if (in != NULL)
            fclose(in);

        perror(temp);
        return 0;
    }

    if (in != NULL) {
        while (fgets(line, sizeof(line), in) != NULL) {
            char copy[LINE_MAX];

            snprintf(copy, sizeof(copy), "%s", line);

            if (parse_line(copy, &entry) &&
                strcmp(entry.word, target) == 0) {

                matches++;

                /* Delete: skip the matching record. */
                if (mode == 2)
                    continue;

                /* Update: write the replacement once. */
                if (mode == 3) {
                    if (matches == 1 &&
                        !write_entry(out, replacement))
                        ok = 0;

                    continue;
                }
            }

            /* Preserve records that are not being removed. */
            if (fputs(line, out) == EOF)
                ok = 0;
        }

        if (ferror(in))
            ok = 0;

        fclose(in);
    }

    /* Add only when the word doesn't already exist. */
    if (mode == 1) {
        if (matches != 0)
            ok = 0;
        else if (!write_entry(out, replacement))
            ok = 0;
    }

    /* Update/delete require an existing matching word. */
    if ((mode == 2 || mode == 3) && matches == 0)
        ok = 0;

    if (fclose(out) != 0)
        ok = 0;

    if (!ok) {
        remove(temp);
        return 0;
    }

    /*
       Windows rename() usually cannot replace an existing file.
       Remove the old file before renaming the temporary file.
    */
    if (remove(path) != 0) {
        remove(temp);
        perror(path);
        return 0;
    }

    if (rename(temp, path) != 0) {
        perror("rename");
        remove(temp);
        return 0;
    }

    return 1;
}


/* Validate a word and prepare a safe local copy. */
static int prepare_word(const Word *source,
                        Word *copy,
                        char *path,
                        size_t path_size)
{
    *copy = *source;

    normalize_word(copy->word);

    if (copy->word[0] == '\0' ||
        !word_file_path(copy->word, path,
                        (unsigned long)path_size)) {
        puts("Word must begin with an English letter A-Z.");
        return 0;
    }

    return 1;
}


/* Add a new dictionary entry. */
int dictionary_add(const Word *word)
{
    Word copy;
    char path[256];

    if (!prepare_word(word, &copy, path, sizeof(path)))
        return 0;

    if (!rewrite_file(path, copy.word, &copy, 1)) {
        printf("Could not add '%s'. It may already exist.\n",
               copy.word);
        return 0;
    }

    printf("Added '%s'.\n", copy.word);
    return 1;
}


/* Update an existing dictionary entry. */
int dictionary_update(const Word *word)
{
    Word copy;
    char path[256];

    if (!prepare_word(word, &copy, path, sizeof(path)))
        return 0;

    if (!rewrite_file(path, copy.word, &copy, 3)) {
        printf("Could not update '%s'. Word may not exist.\n",
               copy.word);
        return 0;
    }

    printf("Updated '%s'.\n", copy.word);
    return 1;
}


/* Delete all entries matching the requested word. */
int dictionary_delete(const char *word)
{
    char target[WORD_MAX];
    char path[256];

    snprintf(target, sizeof(target), "%s",
             word ? word : "");

    normalize_word(target);

    if (!word_file_path(target, path, sizeof(path))) {
        puts("Invalid word.");
        return 0;
    }

    if (!rewrite_file(path, target, NULL, 2)) {
        printf("Could not delete '%s'. Word may not exist.\n",
               target);
        return 0;
    }

    printf("Deleted '%s'.\n", target);
    return 1;
}