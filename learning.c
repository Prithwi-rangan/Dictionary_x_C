#include "wordmate.h"

/* Check both the current word|meaning format and older word-only list rows. */
static int word_is_saved(const char *wanted) {
    FILE *file = fopen(MEMORIZED_FILE, "r");
    char line[LINE_MAX];
    if (!file) return 0;
    while (read_file_line(file, line, sizeof(line))) {
        char *separator = strchr(line, '|');
        if (separator) *separator = '\0';
        normalize_word(line);
        if (strcmp(line, wanted) == 0) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

/* Look up a legacy saved word's meaning in the appropriate A-Z text file. */
static int lookup_meaning(const char *word, char *meaning, size_t size) {
    char path[256];
    char line[LINE_MAX];
    if (!word_file_path(word, path, sizeof(path))) return 0;
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    while (read_file_line(file, line, sizeof(line))) {
        char *first = strchr(line, '\t');
        if (!first) continue;
        *first++ = '\0';
        if (strcmp(line, word) == 0) {
            char *second = strchr(first, '\t');
            if (second) *second = '\0';
            snprintf(meaning, size, "%s", first);
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

/* Save a known dictionary entry with its meaning, avoiding duplicate headwords. */
int memorize_word(const char *word) {
    char normalized[WORD_MAX];
    char path[256];
    FILE *dictionary;
    Word entry;
    int found = 0;

    snprintf(normalized, sizeof(normalized), "%s", word ? word : "");
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) {
        puts("Enter a word beginning with A-Z.");
        return 0;
    }
    if (word_is_saved(normalized)) {
        printf("'%s' is already memorized.\n", normalized);
        return 1;
    }
    dictionary = fopen(path, "r");
    if (!dictionary) return 0;
    char line[LINE_MAX];
    while (read_file_line(dictionary, line, sizeof(line))) {
        char copy[LINE_MAX];
        snprintf(copy, sizeof(copy), "%s", line);
        /* The dictionary's text rows are tab-separated: word, meaning, synonyms. */
        char *first = strchr(copy, '\t');
        if (!first) continue;
        *first++ = '\0';
        char *second = strchr(first, '\t');
        if (second) *second = '\0';
        if (strcmp(copy, normalized) == 0) {
            snprintf(entry.word, sizeof(entry.word), "%s", copy);
            snprintf(entry.meaning, sizeof(entry.meaning), "%s", first);
            found = 1;
            break;
        }
    }
    fclose(dictionary);
    if (!found) {
        printf("No entry found for '%s'.\n", normalized);
        return 0;
    }
    FILE *output = fopen(MEMORIZED_FILE, "a");
    if (!output) {
        perror(MEMORIZED_FILE);
        return 0;
    }
    int ok = fprintf(output, "%s|%s\n", entry.word, entry.meaning) >= 0;
    if (fclose(output) != 0) ok = 0;
    if (ok) printf("Saved '%s' and its meaning to Memorized Words.\n", normalized);
    return ok;
}

/* Display saved meanings, looking them up for entries from the older format. */
int show_memorized(void) {
    FILE *file = fopen(MEMORIZED_FILE, "r");
    char line[LINE_MAX];
    size_t index = 0;
    if (!file) {
        puts("You have no memorized words yet.");
        return 0;
    }
    puts("\n=== Memorized Words ===");
    while (read_file_line(file, line, sizeof(line))) {
        char *separator = strchr(line, '|');
        if (!line[0]) continue;
        if (separator) {
            *separator++ = '\0';
            char looked_up[MEANING_MAX];
            const char *meaning = separator;
            if (!meaning[0] && lookup_meaning(line, looked_up, sizeof(looked_up))) meaning = looked_up;
            printf("%zu. %s - %s\n", ++index, line,
                   meaning[0] ? meaning : "(no meaning recorded)");
        } else {
            /* Older saved lists only held the headword. */
            char looked_up[MEANING_MAX];
            if (lookup_meaning(line, looked_up, sizeof(looked_up)))
                printf("%zu. %s - %s\n", ++index, line, looked_up);
            else
                printf("%zu. %s - (no meaning recorded)\n", ++index, line);
        }
    }
    fclose(file);
    if (!index) puts("You have no memorized words yet.");
    return index > 0;
}

/* Read the separate word|synonyms file used by searches and the CLI. */
int learning_show_synonyms(const char *word) {
    char normalized[WORD_MAX];
    char line[LINE_MAX];
    int found = 0;
    snprintf(normalized, sizeof(normalized), "%s", word ? word : "");
    normalize_word(normalized);
    FILE *file = fopen(SYNONYM_FILE, "r");
    if (!file) {
        printf("No synonyms are recorded for '%s'.\n", normalized);
        return 0;
    }
    while (read_file_line(file, line, sizeof(line))) {
        char *separator = strchr(line, '|');
        if (!separator) separator = strchr(line, '\t');
        if (!separator) continue;
        *separator++ = '\0';
        normalize_word(line);
        if (strcmp(line, normalized) == 0 && separator[0]) {
            printf("Synonyms for %s: %s\n", normalized, separator);
            found = 1;
            break;
        }
    }
    fclose(file);
    if (!found) printf("No synonyms are recorded for '%s'.\n", normalized);
    return found;
}
