#include "wordmate.h"

/* Read one prompt response and remove its trailing newline. */
static int read_line(const char *prompt, char *buffer, size_t size) {
    fputs(prompt, stdout);
    if (!fgets(buffer, (int)size, stdin)) return 0;
    trim_newline(buffer);
    return 1;
}

/* Ask for the headword and meaning used by Add and Update. */
static int enter_word(Word *word, int include_word) {
    if (include_word && !read_line("Word: ", word->word, sizeof(word->word))) return 0;
    if (!read_line("Meaning: ", word->meaning, sizeof(word->meaning))) return 0;
    return 1;
}

/* Pause after an action so the user can choose to continue or quit. */
static int continue_or_exit(void) {
    char input[LINE_MAX];
    if (!read_line("\nPress Enter to continue, or 0 to exit: ", input, sizeof(input))) {
        return 0;
    }
    return strcmp(input, "0") != 0;
}

void run_menu(void) {
    char input[LINE_MAX];

    for (;;) {
        puts("\n=== WordMate ===");
        puts("1. Search word");
        puts("2. Add word");
        puts("3. Update word");
        puts("4. Delete word");
        puts("5. Word of the Day");
        puts("6. Memorized words");
        puts("7. Dictionary statistics");
        puts("8. Vocabulary games");
        puts("9. Activity history");
        puts("0. Exit");

        if (!read_line("Choose: ", input, sizeof(input))) return;
        char *end;
        long selected = strtol(input, &end, 10);
        while (*end && isspace((unsigned char)*end)) ++end;
        if (end == input || *end != '\0' || selected < 0 || selected > 9) {
            puts("Choose a number from the menu.");
            if (!continue_or_exit()) return;
            continue;
        }
        int choice = (int)selected;
        int should_pause = 1;
        Word word = {0};

        /* Each menu choice delegates the actual work to its feature module. */
        switch (choice) {
            case 1:
                if (read_line("Word to search: ", word.word, sizeof(word.word))) {
                    dictionary_search(word.word);
                }
                break;
            case 2:
                if (enter_word(&word, 1)) dictionary_add(&word);
                break;
            case 3:
                if (read_line("Word to update: ", word.word, sizeof(word.word)) &&
                    enter_word(&word, 0)) {
                    dictionary_update(&word);
                }
                break;
            case 4:
                if (read_line("Word to delete: ", word.word, sizeof(word.word))) {
                    dictionary_delete(word.word);
                }
                break;
            case 5:
                dictionary_random();
                break;
            case 6:
                show_memorized();
                break;
            case 7:
                dictionary_stats();
                break;
            case 8:
                should_pause = games_menu();
                break;
            case 9:
                show_activity_history();
                break;
            case 0:
                return;
            default:
                puts("Choose a number from the menu.");
                break;
        }

        if (should_pause && !continue_or_exit()) return;
    }
}

/* Print the supported command-line forms in one place. */
void print_help(const char *program) {
    printf("WordMate - terminal dictionary and vocabulary assistant\n\n"
           "Usage: %s [command] [arguments]\n\n"
           "Commands:\n"
           "  search WORD                  Search for a word\n"
           "  add WORD MEANING             Add a dictionary entry\n"
           "  update WORD MEANING          Replace an entry\n"
           "  delete WORD                  Delete all definitions\n"
           "  random                       Show a random word of the day\n"
           "  memorize WORD | memorized    Save or list memorized words\n"
           "  stats                        Show dictionary statistics\n"
           "  games                        Open the vocabulary games menu\n"
           "  missing-beginner             Play Missing Letters in Beginner mode\n"
           "  missing-advanced             Play Missing Letters in Advanced mode\n"
           "  hangman                      Play Project Hangman\n"
           "  history                      Show activity history\n\n"
           "The interactive menu starts when no command is provided.\n", program);
}
