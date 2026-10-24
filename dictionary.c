#include "wordmate.h"

/* Each binary index row points from one unique headword to its text-file offset. */
typedef struct {
    char word[WORD_MAX];
    long offset;
} IndexRecord;

static int read_word_line(char *line, Word *entry);
static int sort_and_index_file(const char *text_path);

/* Keep separators out of fields because tabs and pipes divide stored records. */
static void clean_field(char *text) {
    for (char *p = text; *p; ++p) {
        if (*p == '\t' || *p == '\n' || *p == '\r' || *p == '|') {
            *p = ' ';
        }
    }
}

/* Store dictionary entries as word<TAB>meaning<TAB>synonyms. */
static int write_entry(FILE *out, const char *word, const char *meaning,
                       const char *synonyms) {
    char clean_word[WORD_MAX];
    char clean_meaning[MEANING_MAX];
    char clean_synonyms[SYNONYMS_MAX];

    snprintf(clean_word, sizeof(clean_word), "%s", word);
    snprintf(clean_meaning, sizeof(clean_meaning), "%s", meaning);
    snprintf(clean_synonyms, sizeof(clean_synonyms), "%s", synonyms ? synonyms : "");
    clean_field(clean_word);
    clean_field(clean_meaning);
    clean_field(clean_synonyms);
    return fprintf(out, "%s\t%s\t%s\n", clean_word, clean_meaning,
                   clean_synonyms) >= 0;
}

/* Import the supplied word list once and split it into A-Z text files. */
/* Wrapped definition lines are joined before each word is written. */
static int import_source(void) {
    FILE *source = fopen("azdictionary.txt", "r");
    FILE *outputs[26] = {0};
    char source_line[LINE_MAX];
    char current_word[WORD_MAX] = "";
    char current_meaning[MEANING_MAX] = "";
    int imported = 0;

    for (int i = 0; i < 26; ++i) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        outputs[i] = fopen(path, "w");
        if (!outputs[i]) {
            for (int j = 0; j < 26; ++j) {
                if (outputs[j]) fclose(outputs[j]);
            }
            if (source) fclose(source);
            return 0;
        }
    }

    if (!source) {
        puts("azdictionary.txt was not found; the empty dictionary is ready for new words.");
    } else {
        while (read_file_line(source, source_line, sizeof(source_line))) {
            const char *start = source_line;
            const char *open;
            const char *close;
            int header;

            while (*start == ' ' || *start == '\t') ++start;
            open = strchr(start, '(');
            close = open ? strchr(open, ')') : NULL;
            /* Source headings look like "  word (part of speech) definition". */
            header = start != source_line && open && open > start && open[-1] == ' ' &&
                     close && close[1] == ' ' &&
                     (close == open + 1 || memchr(open, '.', (size_t)(close - open)) != NULL);

            if (header) {
                if (current_word[0] && isalpha((unsigned char)current_word[0])) {
                    int letter = tolower((unsigned char)current_word[0]) - 'a';
                    if (letter >= 0 && letter < 26 &&
                        write_entry(outputs[letter], current_word, current_meaning, "")) {
                        imported++;
                    }
                }
                size_t word_length = (size_t)(open - start - 1);
                if (word_length >= sizeof(current_word)) word_length = sizeof(current_word) - 1;
                memcpy(current_word, start, word_length);
                current_word[word_length] = '\0';
                normalize_word(current_word);
                const char *definition = close + 1;
                while (*definition == ' ') ++definition;
                snprintf(current_meaning, sizeof(current_meaning), "%s", definition);
            } else if (current_word[0] && start[0]) {
                size_t used = strlen(current_meaning);
                if (used + 1 < sizeof(current_meaning)) {
                    if (used) current_meaning[used++] = ' ';
                    snprintf(current_meaning + used, sizeof(current_meaning) - used,
                             "%s", start);
                }
            } else if (current_word[0]) {
                int letter = tolower((unsigned char)current_word[0]) - 'a';
                if (letter >= 0 && letter < 26 &&
                    write_entry(outputs[letter], current_word, current_meaning, "")) {
                    imported++;
                }
                current_word[0] = '\0';
                current_meaning[0] = '\0';
            }
        }
        if (current_word[0] && isalpha((unsigned char)current_word[0])) {
            int letter = tolower((unsigned char)current_word[0]) - 'a';
            if (letter >= 0 && letter < 26 &&
                write_entry(outputs[letter], current_word, current_meaning, "")) {
                imported++;
            }
        }
        fclose(source);
        printf("Imported %d dictionary definitions from azdictionary.txt.\n", imported);
    }

    int ok = 1;
    for (int i = 0; i < 26; ++i) {
        if (fclose(outputs[i]) != 0) ok = 0;
    }
    return ok;
}

/* Compare only the headword portion of two tab-separated dictionary rows. */
static int compare_words(const char *left, const char *right) {
    while (*left && *right && *left != '\t' && *right != '\t') {
        int a = tolower((unsigned char)*left++);
        int b = tolower((unsigned char)*right++);
        if (a != b) return a - b;
    }
    if (*left != '\t' && *left != '\0') return 1;
    if (*right != '\t' && *right != '\0') return -1;
    return 0;
}

/* qsort passes pointers to the allocated row strings, not the strings directly. */
static int record_compare(const void *left, const void *right) {
    return compare_words(*(const char *const *)left, *(const char *const *)right);
}

/* Keep an independent copy of a row while the source file is closed and sorted. */
static char *copy_line(const char *line) {
    size_t length = strlen(line) + 1;
    char *copy = (char *)malloc(length);
    if (copy) memcpy(copy, line, length);
    return copy;
}

/* Split a stored row into its three fields; missing synonyms are allowed. */
static int read_word_line(char *line, Word *entry) {
    char *first;
    char *second;

    trim_newline(line);
    first = strchr(line, '\t');
    if (!first) return 0;
    *first++ = '\0';
    second = strchr(first, '\t');
    if (second) {
        *second++ = '\0';
        snprintf(entry->synonyms, sizeof(entry->synonyms), "%s", second);
    } else {
        entry->synonyms[0] = '\0';
    }
    snprintf(entry->word, sizeof(entry->word), "%s", line);
    snprintf(entry->meaning, sizeof(entry->meaning), "%s", first);
    return entry->word[0] != '\0';
}

/* Sort one letter file, then write unique word/offset pairs for binary search. */
static int sort_and_index_file(const char *text_path) {
    FILE *input = fopen(text_path, "r");
    char line[LINE_MAX];
    char **records = NULL;
    size_t count = 0;
    size_t allocated = 0;
    int ok = 1;

    if (!input) {
        input = fopen(text_path, "w");
        if (!input) return 0;
        fclose(input);
        input = fopen(text_path, "r");
        if (!input) return 0;
    }

    while (read_file_line(input, line, sizeof(line))) {
        if (count == allocated) {
            size_t next = allocated ? allocated * 2 : 256;
            char **larger = (char **)realloc(records, next * sizeof(*records));
            if (!larger) {
                ok = 0;
                break;
            }
            records = larger;
            allocated = next;
        }
        records[count] = copy_line(line);
        if (!records[count]) {
            ok = 0;
            break;
        }
        count++;
    }
    if (ferror(input)) ok = 0;
    fclose(input);
    if (!ok) {
        for (size_t i = 0; i < count; ++i) free(records[i]);
        free(records);
        return 0;
    }

    /* Sorting the readable file lets the index use the same alphabetical order. */
    if (count > 1) qsort(records, count, sizeof(*records), record_compare);
    char sorted_path[320];
    snprintf(sorted_path, sizeof(sorted_path), "%s.sort.tmp", text_path);
    FILE *sorted = fopen(sorted_path, "w");
    if (!sorted) ok = 0;
    for (size_t i = 0; ok && i < count; ++i) {
        if (fprintf(sorted, "%s\n", records[i]) < 0) ok = 0;
    }
    if (sorted && fclose(sorted) != 0) ok = 0;
    for (size_t i = 0; i < count; ++i) free(records[i]);
    free(records);
    if (!ok || rename(sorted_path, text_path) != 0) {
        remove(sorted_path);
        return 0;
    }

    char index_path[320];
    char index_temp[340];
    snprintf(index_path, sizeof(index_path), "%.*s.idx",
             (int)(strlen(text_path) - 4), text_path);
    snprintf(index_temp, sizeof(index_temp), "%s.tmp", index_path);
    FILE *text = fopen(text_path, "r");
    FILE *index = fopen(index_temp, "wb");
    if (!text || !index) {
        if (text) fclose(text);
        if (index) fclose(index);
        remove(index_temp);
        return 0;
    }

    char previous[WORD_MAX] = "";
    Word entry;
    /* Record each distinct word's byte offset in the sorted text file. */
    while (1) {
        long offset = ftell(text);
        if (offset < 0) { ok = 0; break; }
        if (!read_file_line(text, line, sizeof(line))) break;
        char copy[LINE_MAX];
        snprintf(copy, sizeof(copy), "%s", line);
        if (!read_word_line(copy, &entry) || strcmp(previous, entry.word) == 0) continue;
        IndexRecord record = {0};
        snprintf(record.word, sizeof(record.word), "%s", entry.word);
        record.offset = offset;
        if (fwrite(&record, sizeof(record), 1, index) != 1) { ok = 0; break; }
        snprintf(previous, sizeof(previous), "%s", entry.word);
    }
    if (ferror(text)) ok = 0;
    fclose(text);
    if (fclose(index) != 0) ok = 0;
    if (!ok || rename(index_temp, index_path) != 0) {
        remove(index_temp);
        return 0;
    }
    return 1;
}

/* Build (or rebuild) the sorted text and binary index for every letter. */
static int build_all_indexes(void) {
    for (int i = 0; i < 26; ++i) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        if (!sort_and_index_file(path)) return 0;
    }
    return 1;
}

/* Migrate inline synonyms into synonym.txt the first time the shared file is needed. */
static int initialize_synonym_file(void) {
    FILE *existing = fopen(SYNONYM_FILE, "r");
    char line[LINE_MAX];
    int has_rows = 0;
    if (existing) {
        while (read_file_line(existing, line, sizeof(line))) {
            if (line[0]) { has_rows = 1; break; }
        }
        fclose(existing);
    }
    if (has_rows) return 1;

    char temporary[96];
    snprintf(temporary, sizeof(temporary), "%s.tmp", SYNONYM_FILE);
    FILE *output = fopen(temporary, "w");
    if (!output) return 0;
    for (int i = 0; i < 26; ++i) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        FILE *input = fopen(path, "r");
        if (!input) continue;
        while (read_file_line(input, line, sizeof(line))) {
            Word entry;
            char copy[LINE_MAX];
            snprintf(copy, sizeof(copy), "%s", line);
            if (read_word_line(copy, &entry) && entry.synonyms[0] &&
                fprintf(output, "%s|%s\n", entry.word, entry.synonyms) < 0) {
                fclose(input);
                fclose(output);
                remove(temporary);
                return 0;
            }
        }
        if (ferror(input)) {
            fclose(input);
            fclose(output);
            remove(temporary);
            return 0;
        }
        fclose(input);
    }
    if (fclose(output) != 0 || rename(temporary, SYNONYM_FILE) != 0) {
        remove(temporary);
        return 0;
    }
    return 1;
}

/* Import data when needed and ensure all 26 letter files have matching indexes. */
int ensure_data_ready(void) {
    char marker[64];
    char version[64];
    FILE *check;
    int initialized;
    int indexed;

    snprintf(marker, sizeof(marker), DICTIONARY_DIR "/.initialized");
    snprintf(version, sizeof(version), DICTIONARY_DIR "/.index-v1");
    check = fopen(marker, "r");
    initialized = check != NULL;
    if (check) fclose(check);
    if (!initialized && !import_source()) return 0;

    check = fopen(version, "r");
    indexed = check != NULL;
    if (check) fclose(check);
    if (indexed) {
        for (int i = 0; i < 26; ++i) {
            char path[64];
            snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.idx", 'a' + i);
            check = fopen(path, "rb");
            if (!check) { indexed = 0; break; }
            fclose(check);
        }
    }
    if (!indexed && !build_all_indexes()) return 0;

    if (!initialized) {
        FILE *done = fopen(marker, "w");
        if (!done) return 0;
        fputs("WordMate dictionary initialized.\n", done);
        fclose(done);
    }
    if (!indexed) {
        FILE *done = fopen(version, "w");
        if (!done) return 0;
        fputs("Sorted A-Z files with binary-search indexes.\n", done);
        fclose(done);
    }
    return initialize_synonym_file();
}

/* Print the dictionary fields; synonyms are read from the shared synonym file. */
static void display_entry(const Word *entry, int show_synonyms) {
    (void)show_synonyms;
    printf("\n%s\n  %s\n", entry->word,
           entry->meaning[0] ? entry->meaning : "(no meaning recorded)");
}

/* Binary-search the selected letter's index, then read its text record by offset. */
int dictionary_search(const char *word, int show_synonyms) {
    char normalized[WORD_MAX];
    char path[256];
    char index_path[256];
    IndexRecord record;
    long target_offset = -1;

    snprintf(normalized, sizeof(normalized), "%s", word ? word : "");
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) {
        puts("Enter a word beginning with A-Z.");
        return 0;
    }
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
    if (bytes < 0 || bytes % (long)sizeof(record) != 0) {
        fclose(index); fclose(file); return 0;
    }
    size_t count = (size_t)(bytes / (long)sizeof(record));
    size_t low = 0;
    size_t high = count;
    /* The index is sorted by word, so each comparison halves the remaining range. */
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (fseek(index, (long)(middle * sizeof(record)), SEEK_SET) != 0 ||
            fread(&record, sizeof(record), 1, index) != 1) break;
        int order = strcmp(normalized, record.word);
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

    int found = 0;
    char line[LINE_MAX];
    Word entry;
    while (read_file_line(file, line, sizeof(line))) {
        if (!read_word_line(line, &entry)) continue;
        int order = strcmp(entry.word, normalized);
        if (order == 0) {
            display_entry(&entry, show_synonyms);
            found = 1;
        } else if (found || order > 0) {
            break;
        }
    }
    fclose(file);
    if (!found) {
        printf("No entry found for '%s'.\n", normalized);
        dictionary_suggest(normalized);
    } else if (show_synonyms) {
        learning_show_synonyms(normalized);
    }
    return found;
}

/* Rewrite a letter file through a temporary file; mode 1=add, 2=delete, 3=update. */
static int rewrite_letter(const char *path, const char *target,
                          const Word *replacement, int mode) {
    FILE *input = fopen(path, "r");
    char temp[320];
    char line[LINE_MAX];
    int matches = 0;
    int ok = 1;

    if (!input && mode != 1) return 0;
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    FILE *output = fopen(temp, "w");
    if (!output) { if (input) fclose(input); return 0; }

    Word entry;
    if (input) {
        while (read_file_line(input, line, sizeof(line))) {
            char copy[LINE_MAX];
            snprintf(copy, sizeof(copy), "%s", line);
            if (read_word_line(copy, &entry) && strcmp(entry.word, target) == 0) {
                matches++;
                if (mode == 2 || mode == 3) {
                    if (mode == 3 && matches == 1 &&
                        !write_entry(output, replacement->word, replacement->meaning,
                                     replacement->synonyms)) ok = 0;
                    continue;
                }
            }
            if (fprintf(output, "%s\n", line) < 0) ok = 0;
        }
        if (ferror(input)) ok = 0;
        fclose(input);
    }
    /* Reject duplicates on add and missing words on update/delete. */
    if (mode == 1) {
        if (matches) ok = 0;
        else if (!write_entry(output, replacement->word, replacement->meaning,
                              replacement->synonyms)) ok = 0;
    } else if ((mode == 2 || mode == 3) && matches == 0) {
        ok = 0;
    }
    if (fclose(output) != 0) ok = 0;
    if (ok && rename(temp, path) != 0) ok = 0;
    if (!ok) remove(temp);
    return ok ? sort_and_index_file(path) : 0;
}

/* Keep synonym.txt sorted while replacing or removing one headword's synonyms. */
static int rewrite_synonym_file(const char *word, const char *synonyms) {
    FILE *input = fopen(SYNONYM_FILE, "r");
    FILE *output;
    char temp[96];
    char line[LINE_MAX];
    int inserted = 0;
    int found = 0;
    int ok = 1;

    snprintf(temp, sizeof(temp), "%s.tmp", SYNONYM_FILE);
    output = fopen(temp, "w");
    if (!output) { if (input) fclose(input); return 0; }
    if (input) {
        while (read_file_line(input, line, sizeof(line))) {
            char *separator = strchr(line, '|');
            if (!separator) separator = strchr(line, '\t');
            if (separator) {
                *separator = '\0';
                if (strcmp(line, word) == 0) {
                    found = 1;
                    continue;
                }
                if (!inserted && synonyms[0] && strcmp(line, word) > 0) {
                    if (fprintf(output, "%s|%s\n", word, synonyms) < 0) ok = 0;
                    inserted = 1;
                }
                if (fprintf(output, "%s|%s\n", line, separator + 1) < 0) ok = 0;
            }
        }
        if (ferror(input)) ok = 0;
        fclose(input);
    }
    (void)found;
    if (synonyms[0] && !inserted && fprintf(output, "%s|%s\n", word, synonyms) < 0) ok = 0;
    if (fclose(output) != 0) ok = 0;
    if (ok && rename(temp, SYNONYM_FILE) != 0) ok = 0;
    if (!ok) remove(temp);
    return ok;
}

/* Normalize and sanitize all user-entered fields before writing them to files. */
static int normalize_entry(Word *word) {
    if (!word || !word->word[0]) return 0;
    normalize_word(word->word);
    if (!isalpha((unsigned char)word->word[0])) return 0;
    clean_field(word->word);
    clean_field(word->meaning);
    clean_field(word->synonyms);
    return 1;
}

/* Add a new entry, then synchronize its optional synonyms with synonym.txt. */
int dictionary_add(const Word *word) {
    char normalized[WORD_MAX];
    char path[256];
    Word copy = *word;
    if (!normalize_entry(&copy)) return 0;
    snprintf(normalized, sizeof(normalized), "%s", copy.word);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    if (!rewrite_letter(path, normalized, &copy, 1)) {
        printf("'%s' already exists or could not be saved. Use update to replace it.\n", normalized);
        return 0;
    }
    rewrite_synonym_file(normalized, copy.synonyms);
    printf("Added '%s'.\n", normalized);
    return 1;
}

/* Replace the existing row so the displayed meaning cannot remain stale. */
int dictionary_update(const Word *word) {
    char normalized[WORD_MAX];
    char path[256];
    Word copy = *word;
    if (!normalize_entry(&copy)) return 0;
    snprintf(normalized, sizeof(normalized), "%s", copy.word);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    if (!rewrite_letter(path, normalized, &copy, 3)) {
        printf("No entry found for '%s'.\n", normalized);
        return 0;
    }
    rewrite_synonym_file(normalized, copy.synonyms);
    printf("Updated '%s'.\n", normalized);
    return 1;
}

/* Remove every row for a headword and its matching shared synonym record. */
int dictionary_delete(const char *word) {
    char normalized[WORD_MAX];
    char path[256];
    snprintf(normalized, sizeof(normalized), "%s", word ? word : "");
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    if (!rewrite_letter(path, normalized, NULL, 2)) {
        printf("No entry found for '%s'.\n", normalized);
        return 0;
    }
    rewrite_synonym_file(normalized, "");
    printf("Deleted all definitions for '%s'.\n", normalized);
    return 1;
}

/* Compare positions after the fixed first letter and return a percentage match. */
static int positional_similarity(const char *search, const char *candidate) {
    size_t search_length = strlen(search);
    size_t candidate_length = strlen(candidate);
    size_t longest = search_length > candidate_length ? search_length : candidate_length;
    int matches = 0;

    if (!search_length || !candidate_length || search[0] != candidate[0]) return 0;
    if (longest <= 1) return 100;
    for (size_t i = 1; i < search_length && i < candidate_length; ++i) {
        if (search[i] == candidate[i]) matches++;
    }
    return matches * 100 / (int)(longest - 1);
}

/* Suggest same-first-letter entries whose remaining positions match over 70%. */
int dictionary_suggest(const char *word) {
    char normalized[WORD_MAX];
    char path[64];
    char previous_word[WORD_MAX] = "";
    Word suggestions[10] = {{0}};
    size_t suggestion_count = 0;
    Word exact_entry = {0};
    int exact_found = 0;

    snprintf(normalized, sizeof(normalized), "%s", word ? word : "");
    normalize_word(normalized);
    if (!word_file_path(normalized, path, sizeof(path))) return 0;
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char line[LINE_MAX];
    Word entry;
    while (read_file_line(file, line, sizeof(line))) {
        char copy[LINE_MAX];
        snprintf(copy, sizeof(copy), "%s", line);
        if (!read_word_line(copy, &entry)) continue;
        /* A headword may have several definitions, but should appear only once. */
        if (strcmp(entry.word, previous_word) == 0) continue;
        snprintf(previous_word, sizeof(previous_word), "%s", entry.word);
        /* An exact match confirms the spelling and should show its meaning. */
        if (strcmp(entry.word, normalized) == 0) {
            exact_entry = entry;
            exact_found = 1;
            continue;
        }
        if (positional_similarity(normalized, entry.word) > 70 && suggestion_count < 10)
            suggestions[suggestion_count++] = entry;
    }
    fclose(file);
    if (exact_found) {
        puts("The spelling is correct:");
        display_entry(&exact_entry, 0);
        return 1;
    }
    if (!suggestion_count) {
        puts("No spelling suggestions were found.");
        return 0;
    }

    /* Number suggestions so the user can select one and see its meaning. */
    puts("Did you mean:");
    for (size_t i = 0; i < suggestion_count; ++i)
        printf("%zu. %s\n", i + 1, suggestions[i].word);

    char input[LINE_MAX];
    long selected;
    for (;;) {
        char *end;
        fputs("Choose a number to see its meaning (0 to cancel): ", stdout);
        if (!fgets(input, sizeof(input), stdin)) return 0;
        trim_newline(input);
        selected = strtol(input, &end, 10);
        while (*end && isspace((unsigned char)*end)) ++end;
        if (end != input && *end == '\0' && selected >= 0 &&
            selected <= (long)suggestion_count) break;
        printf("Enter a number from 0 to %zu.\n", suggestion_count);
    }
    if (selected == 0) {
        puts("No suggestion selected.");
        return 1;
    }

    display_entry(&suggestions[selected - 1], 0);
    return 1;
}

/* Count fixed-size index rows so random selection can use a flat range. */
static size_t index_record_count(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return 0; }
    long bytes = ftell(file);
    fclose(file);
    if (bytes <= 0 || bytes % (long)sizeof(IndexRecord) != 0) return 0;
    return (size_t)(bytes / (long)sizeof(IndexRecord));
}

/* Choose a random indexed word across the A-Z files for Word of the Day. */
int dictionary_random(void) {
    size_t total = 0;
    char path[64];
    for (int i = 0; i < 26; ++i) {
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.idx", 'a' + i);
        total += index_record_count(path);
    }
    if (!total) { puts("The dictionary is empty."); return 0; }

    size_t selected = (size_t)rand() % total;
    for (int i = 0; i < 26; ++i) {
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.idx", 'a' + i);
        size_t count = index_record_count(path);
        if (selected >= count) { selected -= count; continue; }
        FILE *index = fopen(path, "rb");
        if (!index) return 0;
        IndexRecord record;
        int ok = fseek(index, (long)(selected * sizeof(record)), SEEK_SET) == 0 &&
                 fread(&record, sizeof(record), 1, index) == 1;
        fclose(index);
        if (!ok) return 0;
        char text_path[64];
        snprintf(text_path, sizeof(text_path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        FILE *text = fopen(text_path, "r");
        if (!text || fseek(text, record.offset, SEEK_SET) != 0) {
            if (text) fclose(text);
            return 0;
        }
        char line[LINE_MAX];
        Word entry;
        if (read_file_line(text, line, sizeof(line)) && read_word_line(line, &entry)) {
            puts("Word of the Day:");
            display_entry(&entry, 0);
            fclose(text);
            return 1;
        }
        fclose(text);
        return 0;
    }
    return 0;
}

/* Count comma-separated synonym names while ignoring empty items and spaces. */
static int count_synonyms_in_field(const char *text) {
    int count = 0;
    int has_character = 0;
    for (const char *p = text; ; ++p) {
        if (*p == ',' || *p == '\0') {
            if (has_character) count++;
            has_character = 0;
            if (*p == '\0') break;
        } else if (!isspace((unsigned char)*p)) {
            has_character = 1;
        }
    }
    return count;
}

/* Count non-empty rows in the user's memorized-word list. */
static size_t count_memorized_words(void) {
    FILE *file = fopen(MEMORIZED_FILE, "r");
    char line[LINE_MAX];
    size_t count = 0;
    if (!file) return 0;
    while (read_file_line(file, line, sizeof(line))) {
        if (line[0]) count++;
    }
    fclose(file);
    return count;
}

/* Summarize indexed headwords, memorized entries, and recorded synonyms. */
int dictionary_stats(void) {
    size_t words = 0;
    size_t synonyms = 0;
    char path[64];
    for (int i = 0; i < 26; ++i) {
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.idx", 'a' + i);
        words += index_record_count(path);
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + i);
        FILE *file = fopen(path, "r");
        if (!file) continue;
        char line[LINE_MAX];
        Word entry;
        while (read_file_line(file, line, sizeof(line))) {
            if (read_word_line(line, &entry)) synonyms += count_synonyms_in_field(entry.synonyms);
        }
        fclose(file);
    }
    FILE *synonym_file = fopen(SYNONYM_FILE, "r");
    if (synonym_file) {
        char line[LINE_MAX];
        size_t records = 0;
        while (read_file_line(synonym_file, line, sizeof(line))) {
            if (strchr(line, '|')) records++;
        }
        fclose(synonym_file);
        if (records) {
            synonyms = 0;
            synonym_file = fopen(SYNONYM_FILE, "r");
            while (synonym_file && read_file_line(synonym_file, line, sizeof(line))) {
                char *separator = strchr(line, '|');
                if (separator) synonyms += count_synonyms_in_field(separator + 1);
            }
            if (synonym_file) fclose(synonym_file);
        }
    }
    printf("Dictionary words: %zu\nMemorized words: %zu\nSynonyms: %zu\nBuild date: %s\n",
           words, count_memorized_words(), synonyms, __DATE__);
    return 1;
}

/* Build a bounded pool of suitable unique words and meanings for the games. */
int dictionary_load_game_words(GameWord **words, size_t *count) {
    enum { POOL_LIMIT = 4000 };
    GameWord *pool = NULL;
    size_t capacity = 0;
    size_t candidates = 0;

    if (!words || !count) return 0;
    *words = NULL;
    *count = 0;
    for (int letter = 0; letter < 26; ++letter) {
        char path[64];
        snprintf(path, sizeof(path), DICTIONARY_DIR "/%c.txt", 'a' + letter);
        FILE *file = fopen(path, "r");
        if (!file) continue;
        char line[LINE_MAX];
        char previous[WORD_MAX] = "";
        Word entry;
        while (read_file_line(file, line, sizeof(line))) {
            char copy[LINE_MAX];
            snprintf(copy, sizeof(copy), "%s", line);
            if (!read_word_line(copy, &entry) || strcmp(previous, entry.word) == 0) continue;
            snprintf(previous, sizeof(previous), "%s", entry.word);
            size_t length = strlen(entry.word);
            if (length < 3 || length > 16 || strlen(entry.meaning) < 20) continue;
            int simple = 1;
            for (size_t i = 0; i < length; ++i) {
                if (entry.word[i] < 'a' || entry.word[i] > 'z') simple = 0;
            }
            if (!simple) continue;

            /* Reservoir sampling keeps a random sample without loading all words. */
            candidates++;
            size_t selected = candidates <= POOL_LIMIT ? candidates - 1 : (size_t)rand() % candidates;
            if (selected >= POOL_LIMIT) continue;
            if (selected == *count) {
                if (*count == capacity) {
                    size_t next = capacity ? capacity * 2 : 128;
                    if (next > POOL_LIMIT) next = POOL_LIMIT;
                    GameWord *larger = (GameWord *)realloc(pool, next * sizeof(*pool));
                    if (!larger) {
                        fclose(file); free(pool); *words = NULL; *count = 0; return 0;
                    }
                    pool = larger;
                    capacity = next;
                }
                snprintf(pool[*count].word, sizeof(pool[*count].word), "%s", entry.word);
                snprintf(pool[*count].meaning, sizeof(pool[*count].meaning), "%s", entry.meaning);
                (*count)++;
            } else {
                snprintf(pool[selected].word, sizeof(pool[selected].word), "%s", entry.word);
                snprintf(pool[selected].meaning, sizeof(pool[selected].meaning), "%s", entry.meaning);
            }
        }
        fclose(file);
    }
    if (!*count) {
        free(pool);
        puts("There are not enough suitable dictionary entries for a vocabulary activity.");
        return 0;
    }
    *words = pool;
    return 1;
}
