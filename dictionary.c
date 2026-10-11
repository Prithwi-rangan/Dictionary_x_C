#define _POSIX_C_SOURCE 200809L
#include "wordmate.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    char word[WORD_MAX];
    long offset;
} IndexRecord;

static int read_word_line(char *line, Word *entry);

static int make_directory(const char *path) {
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return 1;
    perror(path);
    return 0;
}

static void clean_field(char *text) {
    for (char *p = text; *p; ++p) {
        if (*p == '\t' || *p == '\n' || *p == '\r' || *p == '|') *p = ' ';
    }
}

static int write_entry(FILE *out, const char *word, const char *meaning, const char *synonyms) {
    char w[WORD_MAX], m[MEANING_MAX], s[SYNONYMS_MAX];
    snprintf(w, sizeof(w), "%s", word);
    snprintf(m, sizeof(m), "%s", meaning);
    snprintf(s, sizeof(s), "%s", synonyms ? synonyms : "");
    clean_field(w); clean_field(m); clean_field(s);
    return fprintf(out, "%s\t%s\t%s\n", w, m, s) >= 0;
}

/* Import the supplied Webster-style text file once into human-readable A-Z files. */
static int import_source(void) {
    FILE *source = fopen("azdictionary.txt", "r");
    if (!source) {
        fprintf(stderr, "Could not open azdictionary.txt. Add words manually or place the source file beside wordmate.\n");
        return 1;
    }
    FILE *outputs[26] = {0};
    for (int i = 0; i < 26; ++i) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        outputs[i] = fopen(path, "w");
        if (!outputs[i]) {
            perror(path);
            for (int j = 0; j <= i; ++j) if (outputs[j]) fclose(outputs[j]);
            fclose(source);
            return 0;
        }
    }
    char *line = NULL;
    size_t capacity = 0;
    ssize_t got;
    char current_word[WORD_MAX] = "";
    char current_meaning[MEANING_MAX] = "";
    int imported = 0;
    while ((got = getline(&line, &capacity, source)) != -1) {
        (void)got;
        trim_newline(line);
        const char *start = line;
        while (*start == ' ' || *start == '\t') ++start;
        const char *open = strchr(start, '(');
        const char *close = open ? strchr(open, ')') : NULL;
        int header = start != line && open && open > start && open[-1] == ' ' &&
                    close && close[1] == ' ' &&
                    (close == open + 1 || memchr(open, '.', (size_t)(close - open)) != NULL);
        if (header) {
            if (current_word[0] && isalpha((unsigned char)current_word[0])) {
                int i = tolower((unsigned char)current_word[0]) - 'a';
                if (i >= 0 && i < 26 && write_entry(outputs[i], current_word, current_meaning, "")) ++imported;
            }
            size_t word_len = (size_t)(open - start - 1);
            if (word_len >= sizeof(current_word)) word_len = sizeof(current_word) - 1;
            memcpy(current_word, start, word_len);
            current_word[word_len] = '\0';
            normalize_word(current_word);
            const char *definition = strchr(open, ')') + 1;
            while (*definition == ' ') ++definition;
            snprintf(current_meaning, sizeof(current_meaning), "%s", definition);
        } else if (current_word[0] && start[0]) {
            size_t used = strlen(current_meaning);
            if (used + 1 < sizeof(current_meaning)) {
                if (used) current_meaning[used++] = ' ';
                snprintf(current_meaning + used, sizeof(current_meaning) - used, "%s", start);
            }
        } else if (current_word[0]) {
            int i = tolower((unsigned char)current_word[0]) - 'a';
            if (i >= 0 && i < 26 && write_entry(outputs[i], current_word, current_meaning, "")) ++imported;
            current_word[0] = current_meaning[0] = '\0';
        }
    }
    if (current_word[0] && isalpha((unsigned char)current_word[0])) {
        int i = tolower((unsigned char)current_word[0]) - 'a';
        if (i >= 0 && i < 26 && write_entry(outputs[i], current_word, current_meaning, "")) ++imported;
    }
    free(line);
    fclose(source);
    for (int i = 0; i < 26; ++i) fclose(outputs[i]);
    printf("Imported %d dictionary definitions from azdictionary.txt.\n", imported);
    return 1;
}

static int record_compare(const void *left, const void *right) {
    const char *a = *(const char *const *)left;
    const char *b = *(const char *const *)right;
    size_t a_len = strcspn(a, "\t\r\n"), b_len = strcspn(b, "\t\r\n");
    size_t common = a_len < b_len ? a_len : b_len;
    int order = strncasecmp(a, b, common);
    if (order) return order;
    if (a_len != b_len) return a_len < b_len ? -1 : 1;
    return strcmp(a, b);
}

static int sort_and_index_file(const char *text_path) {
    FILE *input = fopen(text_path, "r");
    if (!input) {
        input = fopen(text_path, "w");
        if (!input) { perror(text_path); return 0; }
        fclose(input);
        input = fopen(text_path, "r");
        if (!input) return 0;
    }
    char **records = NULL;
    size_t count = 0, allocated = 0;
    char *line = NULL;
    size_t line_capacity = 0;
    int ok = 1;
    while (getline(&line, &line_capacity, input) != -1) {
        if (count == allocated) {
            size_t next = allocated ? allocated * 2 : 256;
            char **grown = realloc(records, next * sizeof(*records));
            if (!grown) { ok = 0; break; }
            records = grown;
            allocated = next;
        }
        records[count] = strdup(line);
        if (!records[count]) { ok = 0; break; }
        ++count;
    }
    if (ferror(input)) ok = 0;
    free(line);
    fclose(input);
    if (!ok) {
        for (size_t i = 0; i < count; ++i) free(records[i]);
        free(records);
        return 0;
    }
    if (count > 1) qsort(records, count, sizeof(*records), record_compare);
    char sorted_temp[320], index_path[320], index_temp[340];
    snprintf(sorted_temp, sizeof(sorted_temp), "%s.sort.tmp", text_path);
    FILE *sorted = fopen(sorted_temp, "w");
    if (!sorted) { perror(sorted_temp); ok = 0; }
    for (size_t i = 0; ok && i < count; ++i)
        if (fputs(records[i], sorted) == EOF) ok = 0;
    if (sorted && fclose(sorted) != 0) ok = 0;
    for (size_t i = 0; i < count; ++i) free(records[i]);
    free(records);
    if (!ok || rename(sorted_temp, text_path) != 0) {
        if (ok) perror("sorting dictionary file");
        remove(sorted_temp);
        return 0;
    }

    snprintf(index_path, sizeof(index_path), "%.*s.idx",
             (int)(strlen(text_path) - 4), text_path);
    snprintf(index_temp, sizeof(index_temp), "%s.tmp", index_path);
    FILE *text = fopen(text_path, "r");
    FILE *index = fopen(index_temp, "wb");
    if (!text || !index) {
        if (text) fclose(text);
        if (index) fclose(index);
        perror(index_temp);
        remove(index_temp);
        return 0;
    }
    line = NULL;
    line_capacity = 0;
    char previous[WORD_MAX] = "";
    Word entry;
    while (1) {
        long offset = ftell(text);
        if (offset < 0) { ok = 0; break; }
        if (getline(&line, &line_capacity, text) == -1) break;
        char copy[LINE_MAX];
        snprintf(copy, sizeof(copy), "%s", line);
        if (!read_word_line(copy, &entry) || strcasecmp(previous, entry.word) == 0) continue;
        IndexRecord record = {0};
        snprintf(record.word, sizeof(record.word), "%s", entry.word);
        record.offset = offset;
        if (fwrite(&record, sizeof(record), 1, index) != 1) { ok = 0; break; }
        snprintf(previous, sizeof(previous), "%s", entry.word);
    }
    if (ferror(text)) ok = 0;
    free(line);
    fclose(text);
    if (fclose(index) != 0) ok = 0;
    if (!ok || rename(index_temp, index_path) != 0) {
        if (ok) perror("updating dictionary index");
        remove(index_temp);
        return 0;
    }
    return 1;
}

static int build_all_indexes(void) {
    for (int i = 0; i < 26; ++i) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        if (!sort_and_index_file(path)) return 0;
    }
    return 1;
}

int ensure_data_ready(void) {
    if (!make_directory(DATA_DIR) || !make_directory(DICTIONARY_DIR)) return 0;
    char marker[64], index_version[64];
    snprintf(marker, sizeof(marker), DICTIONARY_DIR "/.initialized");
    snprintf(index_version, sizeof(index_version), DICTIONARY_DIR "/.index-v1");
    FILE *check = fopen(marker, "r");
    int initialized = check != NULL;
    if (check) fclose(check);
    if (!initialized && !import_source()) return 0;
    check = fopen(index_version, "r");
    int indexed = check != NULL;
    if (check) fclose(check);
    if (indexed) {
        for (int i = 0; i < 26; ++i) {
            char path[64];
            snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.idx", 'a' + i);
            FILE *index = fopen(path, "rb");
            if (!index) { indexed = 0; break; }
            fclose(index);
        }
    }
    if (!indexed && !build_all_indexes()) return 0;
    if (!initialized) {
        FILE *done = fopen(marker, "w");
        if (!done) { perror(marker); return 0; }
        fputs("WordMate dictionary initialized.\n", done);
        fclose(done);
    }
    if (!indexed) {
        FILE *done = fopen(index_version, "w");
        if (!done) { perror(index_version); return 0; }
        fputs("Sorted A-Z files with binary-search indexes.\n", done);
        fclose(done);
    }
    return 1;
}

static int read_word_line(char *line, Word *entry) {
    char *first = strchr(line, '\t');
    if (!first) return 0;
    *first++ = '\0';
    char *second = strchr(first, '\t');
    if (!second) return 0;
    *second++ = '\0';
    trim_newline(second);
    snprintf(entry->word, sizeof(entry->word), "%s", line);
    snprintf(entry->meaning, sizeof(entry->meaning), "%s", first);
    snprintf(entry->synonyms, sizeof(entry->synonyms), "%s", second);
    return 1;
}

static void display_entry(const Word *entry) {
    printf("\n%s\n  %s\n", entry->word, entry->meaning[0] ? entry->meaning : "(no meaning recorded)");
    if (entry->synonyms[0]) printf("  Synonyms: %s\n", entry->synonyms);
}

int dictionary_search(const char *word, int show_synonyms) {
    (void)show_synonyms;
    char normalized[WORD_MAX], path[256];
    snprintf(normalized, sizeof(normalized), "%s", word);
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) {
        fprintf(stderr, "Enter a word beginning with A-Z.\n");
        return 0;
    }
    char index_path[256];
    snprintf(index_path, sizeof(index_path), "%.*s.idx", (int)(strlen(path) - 4), path);
    FILE *index = fopen(index_path, "rb");
    FILE *file = fopen(path, "r");
    if (!index || !file) {
        if (index) fclose(index);
        if (file) fclose(file);
        printf("No entry found for '%s'.\n", normalized);
        return 0;
    }
    if (fseek(index, 0, SEEK_END) != 0) { fclose(index); fclose(file); return 0; }
    long bytes = ftell(index);
    if (bytes < 0) { fclose(index); fclose(file); return 0; }
    size_t record_count = (size_t)bytes / sizeof(IndexRecord);
    size_t low = 0, high = record_count;
    long target_offset = -1;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        IndexRecord record;
        if (fseek(index, (long)(middle * sizeof(record)), SEEK_SET) != 0 ||
            fread(&record, sizeof(record), 1, index) != 1) break;
        int order = strcasecmp(normalized, record.word);
        if (order == 0) { target_offset = record.offset; break; }
        if (order < 0) high = middle;
        else low = middle + 1;
    }
    fclose(index);
    if (target_offset < 0 || fseek(file, target_offset, SEEK_SET) != 0) {
        fclose(file);
        printf("No entry found for '%s'.\n", normalized);
        dictionary_suggest(normalized);
        return 0;
    }
    char *line = NULL;
    size_t capacity = 0;
    int found = 0;
    Word entry;
    while (getline(&line, &capacity, file) != -1) {
        if (!read_word_line(line, &entry)) continue;
        int order = strcasecmp(entry.word, normalized);
        if (order == 0) {
            display_entry(&entry);
            found = 1;
        } else if (found || order > 0) break;
    }
    free(line);
    fclose(file);
    if (!found) {
        printf("No entry found for '%s'.\n", normalized);
        dictionary_suggest(normalized);
    }
    return found;
}

static int rewrite_letter(const char *path, const char *target, const Word *replacement, int mode) {
    FILE *in = fopen(path, "r");
    if (!in && mode != 1) return 0;
    char temp[320];
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    FILE *out = fopen(temp, "w");
    if (!out) { perror(temp); if (in) fclose(in); return 0; }
    char *line = NULL;
    size_t capacity = 0;
    int matches = 0, ok = 1;
    Word entry;
    if (in) {
        while (getline(&line, &capacity, in) != -1) {
            char copy[LINE_MAX];
            snprintf(copy, sizeof(copy), "%s", line);
            if (read_word_line(copy, &entry) && strcasecmp(entry.word, target) == 0) {
                ++matches;
                if (mode == 2) continue;
                if (mode == 3) {
                    if (matches == 1 && !write_entry(out, replacement->word, replacement->meaning, replacement->synonyms)) ok = 0;
                    continue;
                }
            }
            if (fputs(line, out) == EOF) ok = 0;
        }
        fclose(in);
    }
    free(line);
    if (mode == 1) {
        if (matches) ok = 0;
        else if (!write_entry(out, replacement->word, replacement->meaning, replacement->synonyms)) ok = 0;
    } else if ((mode == 2 || mode == 3) && !matches) ok = 0;
    if (fclose(out) != 0) ok = 0;
    if (ok && rename(temp, path) != 0) { perror("rename"); ok = 0; }
    if (!ok) remove(temp);
    return ok ? sort_and_index_file(path) : 0;
}

int dictionary_add(const Word *word) {
    char normalized[WORD_MAX], path[256];
    snprintf(normalized, sizeof(normalized), "%s", word->word);
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    Word copy = *word;
    snprintf(copy.word, sizeof(copy.word), "%s", normalized);
    if (rewrite_letter(path, normalized, &copy, 1)) { printf("Added '%s'.\n", normalized); return 1; }
    printf("'%s' already exists or could not be saved. Use update to replace it.\n", normalized);
    return 0;
}

int dictionary_update(const Word *word) {
    char normalized[WORD_MAX], path[256];
    snprintf(normalized, sizeof(normalized), "%s", word->word);
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    Word copy = *word;
    snprintf(copy.word, sizeof(copy.word), "%s", normalized);
    if (rewrite_letter(path, normalized, &copy, 3)) { printf("Updated '%s'.\n", normalized); return 1; }
    printf("No entry found for '%s'.\n", normalized);
    return 0;
}

int dictionary_delete(const char *word) {
    char normalized[WORD_MAX], path[256];
    snprintf(normalized, sizeof(normalized), "%s", word);
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    if (rewrite_letter(path, normalized, NULL, 2)) { printf("Deleted all definitions for '%s'.\n", normalized); return 1; }
    printf("No entry found for '%s'.\n", normalized);
    return 0;
}

/* Recursive Levenshtein distance; vocabulary lengths are capped by WORD_MAX. */
static int edit_distance(const char *a, const char *b, size_t i, size_t j,
                         int memo[WORD_MAX][WORD_MAX]) {
    if (!a[i]) return (int)strlen(b + j);
    if (!b[j]) return (int)strlen(a + i);
    if (memo[i][j] >= 0) return memo[i][j];
    int substitution = edit_distance(a, b, i + 1, j + 1, memo) + (tolower((unsigned char)a[i]) != tolower((unsigned char)b[j]));
    int deletion = edit_distance(a, b, i + 1, j, memo) + 1;
    int insertion = edit_distance(a, b, i, j + 1, memo) + 1;
    int best = substitution < deletion ? substitution : deletion;
    memo[i][j] = best < insertion ? best : insertion;
    return memo[i][j];
}

typedef struct { char word[WORD_MAX]; int distance; } Suggestion;

int dictionary_suggest(const char *word) {
    char normalized[WORD_MAX];
    snprintf(normalized, sizeof(normalized), "%s", word);
    normalize_word(normalized);
    Suggestion best[5] = {{{0}, 999}};
    int memo[WORD_MAX][WORD_MAX];
    for (int letter = 0; letter < 26; ++letter) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + letter);
        FILE *file = fopen(path, "r");
        if (!file) continue;
        char *line = NULL; size_t capacity = 0; Word entry;
        while (getline(&line, &capacity, file) != -1) {
            if (!read_word_line(line, &entry) || strcasecmp(entry.word, normalized) == 0) continue;
            size_t a_len = strlen(normalized), b_len = strlen(entry.word);
            if (a_len > b_len + 2 || b_len > a_len + 2 || b_len >= WORD_MAX) continue;
            for (size_t row = 0; row <= a_len; ++row)
                memset(memo[row], 0xff, (b_len + 1) * sizeof(memo[row][0]));
            int distance = edit_distance(normalized, entry.word, 0, 0, memo);
            if (distance > 2) continue;
            int duplicate = 0;
            for (int i = 0; i < 5; ++i) if (best[i].word[0] && strcasecmp(best[i].word, entry.word) == 0) duplicate = 1;
            if (duplicate) continue;
            for (int i = 0; i < 5; ++i) if (distance < best[i].distance) {
                for (int j = 4; j > i; --j) best[j] = best[j - 1];
                snprintf(best[i].word, sizeof(best[i].word), "%s", entry.word);
                best[i].distance = distance;
                break;
            }
        }
        free(line); fclose(file);
    }
    int found = 0;
    for (int i = 0; i < 5; ++i) if (best[i].word[0]) found = 1;
    if (found) {
        printf("Did you mean: ");
        for (int i = 0; i < 5 && best[i].word[0]; ++i) printf("%s%s", best[i].word, best[i + 1].word[0] ? ", " : "?\n");
    }
    return found;
}

int dictionary_random(void) {
    static int seeded;
    if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }
    size_t count = 0;
    char path[64];
    for (int i = 0; i < 26; ++i) {
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        FILE *f = fopen(path, "r"); if (!f) continue;
        char *line = NULL; size_t cap = 0;
        while (getline(&line, &cap, f) != -1) ++count;
        free(line); fclose(f);
    }
    if (!count) { puts("The dictionary is empty."); return 0; }
    size_t selected = (size_t)rand() % count;
    for (int i = 0; i < 26; ++i) {
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        FILE *f = fopen(path, "r"); if (!f) continue;
        char *line = NULL; size_t cap = 0; Word entry;
        while (getline(&line, &cap, f) != -1) if (selected-- == 0) {
            if (read_word_line(line, &entry)) { puts("Word of the Day:"); display_entry(&entry); memorize_word(entry.word); free(line); fclose(f); return 1; }
        }
        free(line); fclose(f);
    }
    return 0;
}

int dictionary_stats(void) {
    size_t entries = 0, synonym_entries = 0, memorized = 0;
    char path[64];
    for (int i = 0; i < 26; ++i) {
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        FILE *f = fopen(path, "r"); if (!f) continue;
        char *line = NULL; size_t cap = 0;
        while (getline(&line, &cap, f) != -1) { ++entries; if (strchr(line, '\t') && strrchr(line, '\t')[1] != '\n') ++synonym_entries; }
        free(line); fclose(f);
    }
    FILE *m = fopen(MEMORIZED_FILE, "r");
    if (m) { char *line = NULL; size_t cap = 0; while (getline(&line, &cap, m) != -1) ++memorized; free(line); fclose(m); }
    printf("Dictionary definitions: %zu\nMemorized words: %zu\nEntries with synonyms: %zu\n", entries, memorized, synonym_entries);
    return 1;
}

int dictionary_load_game_words(GameWord **words, size_t *count) {
    enum { POOL_LIMIT = 4000 };
    if (!words || !count) return 0;
    *words = NULL;
    *count = 0;
    size_t capacity = 0, candidates = 0;
    GameWord *pool = NULL;
    static int seeded;
    if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }
    for (int letter = 0; letter < 26; ++letter) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + letter);
        FILE *file = fopen(path, "r");
        if (!file) continue;
        char *line = NULL;
        size_t line_capacity = 0;
        char previous[WORD_MAX] = "";
        Word entry;
        while (getline(&line, &line_capacity, file) != -1) {
            char copy[LINE_MAX];
            snprintf(copy, sizeof(copy), "%s", line);
            if (!read_word_line(copy, &entry) || strcasecmp(previous, entry.word) == 0) continue;
            snprintf(previous, sizeof(previous), "%s", entry.word);
            size_t length = strlen(entry.word);
            if (length < 3 || length > 16 || strlen(entry.meaning) < 20) continue;
            int simple = 1;
            for (size_t i = 0; i < length; ++i)
                if (entry.word[i] < 'a' || entry.word[i] > 'z') simple = 0;
            if (!simple) continue;
            ++candidates;
            size_t selected = candidates <= POOL_LIMIT ? candidates - 1 : (size_t)rand() % candidates;
            if (selected >= POOL_LIMIT) continue;
            if (selected == *count) {
                if (*count == capacity) {
                    size_t next = capacity ? capacity * 2 : 128;
                    if (next > POOL_LIMIT) next = POOL_LIMIT;
                    GameWord *grown = realloc(pool, next * sizeof(*pool));
                    if (!grown) {
                        free(line); fclose(file); free(pool); *words = NULL; *count = 0;
                        fprintf(stderr, "Not enough memory to load the vocabulary pool.\n");
                        return 0;
                    }
                    pool = grown;
                    capacity = next;
                }
                snprintf(pool[*count].word, sizeof(pool[*count].word), "%s", entry.word);
                snprintf(pool[*count].meaning, sizeof(pool[*count].meaning), "%s", entry.meaning);
                ++*count;
            } else {
                snprintf(pool[selected].word, sizeof(pool[selected].word), "%s", entry.word);
                snprintf(pool[selected].meaning, sizeof(pool[selected].meaning), "%s", entry.meaning);
            }
        }
        free(line);
        fclose(file);
    }
    if (!*count) { free(pool); puts("There are not enough suitable dictionary entries for a vocabulary activity."); return 0; }
    *words = pool;
    return 1;
}
