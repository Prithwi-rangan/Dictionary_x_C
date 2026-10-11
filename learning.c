#define _POSIX_C_SOURCE 200809L
#include "wordmate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int memorize_word(const char *word) {
    char normalized[WORD_MAX];
    snprintf(normalized, sizeof(normalized), "%s", word);
    normalize_word(normalized);
    FILE *in = fopen(MEMORIZED_FILE, "r");
    char *line = NULL; size_t capacity = 0;
    if (in) {
        while (getline(&line, &capacity, in) != -1) {
            trim_newline(line);
            if (strcasecmp(line, normalized) == 0) {
                free(line); fclose(in); printf("'%s' is already memorized.\n", normalized); return 1;
            }
        }
        free(line); fclose(in);
    }
    FILE *out = fopen(MEMORIZED_FILE, "a");
    if (!out) { perror(MEMORIZED_FILE); return 0; }
    int ok = fprintf(out, "%s\n", normalized) >= 0;
    fclose(out);
    if (ok) printf("Saved '%s' to Memorized Words.\n", normalized);
    return ok;
}

int show_memorized(void) {
    FILE *file = fopen(MEMORIZED_FILE, "r");
    if (!file) { puts("You have no memorized words yet."); return 0; }
    char *line = NULL; size_t capacity = 0; size_t index = 0;
    while (getline(&line, &capacity, file) != -1) {
        trim_newline(line);
        if (line[0]) printf("%zu. %s\n", ++index, line);
    }
    free(line); fclose(file);
    if (!index) puts("You have no memorized words yet.");
    return index > 0;
}
