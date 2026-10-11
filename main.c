#define _POSIX_C_SOURCE 200809L
#include "wordmate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void join_arguments(char *out, size_t size, int argc, char **argv, int start) {
    out[0] = '\0';
    for (int i = start; i < argc; ++i) {
        if (i > start) strncat(out, " ", size - strlen(out) - 1);
        strncat(out, argv[i], size - strlen(out) - 1);
    }
}

static int command_line(int argc, char **argv) {
    Command command = {0};
    const char *op = argv[1];
    if (strcmp(op, "search") == 0 && argc >= 3) {
        command.operation = OP_SEARCH;
        snprintf(command.argument.word, sizeof(command.argument.word), "%s", argv[2]);
        dictionary_search(command.argument.word, 1); return 0;
    }
    if (strcmp(op, "add") == 0 && argc >= 4) {
        Word word = {0};
        snprintf(word.word, sizeof(word.word), "%s", argv[2]);
        snprintf(word.meaning, sizeof(word.meaning), "%s", argv[3]);
        if (argc > 4) join_arguments(word.synonyms, sizeof(word.synonyms), argc, argv, 4);
        return dictionary_add(&word) ? 0 : 1;
    }
    if (strcmp(op, "update") == 0 && argc >= 4) {
        Word word = {0};
        snprintf(word.word, sizeof(word.word), "%s", argv[2]);
        snprintf(word.meaning, sizeof(word.meaning), "%s", argv[3]);
        if (argc > 4) join_arguments(word.synonyms, sizeof(word.synonyms), argc, argv, 4);
        return dictionary_update(&word) ? 0 : 1;
    }
    if (strcmp(op, "delete") == 0 && argc >= 3) return dictionary_delete(argv[2]) ? 0 : 1;
    if (strcmp(op, "random") == 0) return dictionary_random() ? 0 : 1;
    if (strcmp(op, "memorize") == 0 && argc >= 3) return memorize_word(argv[2]) ? 0 : 1;
    if (strcmp(op, "memorized") == 0) return show_memorized() ? 0 : 1;
    if (strcmp(op, "stats") == 0) return dictionary_stats() ? 0 : 1;
    if (strcmp(op, "exam") == 0) {
        int questions = argc > 2 ? atoi(argv[2]) : 10;
        int difficulty = 1, mode = 2;
        if (argc > 3) {
            if (strcmp(argv[3], "easy") == 0) difficulty = 0;
            else if (strcmp(argv[3], "hard") == 0) difficulty = 2;
        }
        if (argc > 4) {
            if (strcmp(argv[4], "word") == 0) mode = 0;
            else if (strcmp(argv[4], "meaning") == 0) mode = 1;
        }
        return run_exam(questions, difficulty, mode) ? 0 : 1;
    }
    if (strcmp(op, "guess-word") == 0) return run_guess_word() ? 0 : 1;
    if (strcmp(op, "guess-meaning") == 0) return run_guess_meaning() ? 0 : 1;
    if (strcmp(op, "word-search") == 0) return run_word_search() ? 0 : 1;
    if (strcmp(op, "history") == 0) return show_activity_results() ? 0 : 1;
    if (strcmp(op, "performance") == 0) return show_performance_statistics() ? 0 : 1;
    if (strcmp(op, "games") == 0) { games_menu(); return 0; }
    if (strcmp(op, "vault") == 0) { vault_menu(); return 0; }
    print_help(argv[0]);
    return 2;
}

int main(int argc, char **argv) {
    if (!ensure_data_ready()) return 1;
    if (argc > 1) return command_line(argc, argv);
    run_menu();
    return 0;
}
