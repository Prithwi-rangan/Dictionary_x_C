#include "wordmate.h"

#include <time.h>

/* Join command-line words into one meaning string. */
static void join_arguments(char *out, size_t size, int argc, char **argv, int start) {
    out[0] = '\0';
    for (int i = start; i < argc; ++i) {
        if (i > start && strlen(out) + 1 < size) {
            strncat(out, " ", size - strlen(out) - 1);
        }
        if (strlen(out) + 1 < size) {
            strncat(out, argv[i], size - strlen(out) - 1);
        }
    }
}

/* Run one requested command, or print help when the arguments are invalid. */
static int command_line(int argc, char **argv) {
    const char *operation = argv[1];

    if (strcmp(operation, "search") == 0 && argc >= 3) {
        return dictionary_search(argv[2]) ? 0 : 1;
    }
    if (strcmp(operation, "add") == 0 && argc >= 4) {
        Word word = {0};
        snprintf(word.word, sizeof(word.word), "%s", argv[2]);
        join_arguments(word.meaning, sizeof(word.meaning), argc, argv, 3);
        return dictionary_add(&word) ? 0 : 1;
    }
    if (strcmp(operation, "update") == 0 && argc >= 4) {
        Word word = {0};
        snprintf(word.word, sizeof(word.word), "%s", argv[2]);
        join_arguments(word.meaning, sizeof(word.meaning), argc, argv, 3);
        return dictionary_update(&word) ? 0 : 1;
    }
    if (strcmp(operation, "delete") == 0 && argc >= 3) {
        return dictionary_delete(argv[2]) ? 0 : 1;
    }
    if (strcmp(operation, "random") == 0) return dictionary_random() ? 0 : 1;
    if (strcmp(operation, "memorize") == 0 && argc >= 3) {
        return memorize_word(argv[2]) ? 0 : 1;
    }
    if (strcmp(operation, "memorized") == 0) return show_memorized() ? 0 : 1;
    if (strcmp(operation, "stats") == 0) return dictionary_stats() ? 0 : 1;
    if (strcmp(operation, "missing-beginner") == 0) return run_missing_letters(0) ? 0 : 1;
    if (strcmp(operation, "missing-advanced") == 0) return run_missing_letters(1) ? 0 : 1;
    if (strcmp(operation, "hangman") == 0) return run_hangman() ? 0 : 1;
    if (strcmp(operation, "history") == 0) return show_activity_history() ? 0 : 1;
    if (strcmp(operation, "games") == 0) {
        games_menu();
        return 0;
    }

    print_help(argv[0]);
    return 2;
}

int main(int argc, char **argv) {
    /* Seed the shared random-number generator before games or Word of the Day run. */
    srand((unsigned int)time(NULL));

    /* Prepare the A-Z dictionary files and indexes before accepting any command. */
    if (!ensure_data_ready()) return 1;

    if (argc > 1) return command_line(argc, argv);
    run_menu();
    return 0;
}
