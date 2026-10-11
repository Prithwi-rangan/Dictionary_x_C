#define _POSIX_C_SOURCE 200809L
#include "wordmate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_line(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);
    if (!fgets(buffer, (int)size, stdin)) return 0;
    trim_newline(buffer);
    return 1;
}

static void enter_word(Word *word, int include_word) {
    if (include_word && !read_line("Word: ", word->word, sizeof(word->word))) return;
    read_line("Meaning: ", word->meaning, sizeof(word->meaning));
    read_line("Synonyms (comma separated, optional): ", word->synonyms, sizeof(word->synonyms));
}

void run_menu(void) {
    char input[LINE_MAX];
    for (;;) {
        puts("\n=== WordMate ===\n1. Search word\n2. Add word\n3. Update word\n4. Delete word\n5. Spelling suggestions\n6. Word of the Day\n7. Memorized words\n8. Memorize a word\n9. Dictionary statistics\n10. Secret Vault\n11. Vocabulary games and tests\n12. Activity history\n13. Performance summary\n0. Exit");
        if (!read_line("Choose: ", input, sizeof(input))) break;
        int choice = atoi(input);
        Word word = {0};
        switch (choice) {
            case 1:
                if (read_line("Word to search: ", word.word, sizeof(word.word))) dictionary_search(word.word, 1);
                break;
            case 2:
                enter_word(&word, 1); dictionary_add(&word); break;
            case 3:
                read_line("Word to update: ", word.word, sizeof(word.word)); enter_word(&word, 0); dictionary_update(&word); break;
            case 4:
                read_line("Word to delete: ", word.word, sizeof(word.word)); dictionary_delete(word.word); break;
            case 5:
                read_line("Check spelling of: ", word.word, sizeof(word.word)); dictionary_suggest(word.word); break;
            case 6: dictionary_random(); break;
            case 7: show_memorized(); break;
            case 8:
                read_line("Word to memorize: ", word.word, sizeof(word.word));
                if (dictionary_search(word.word, 1)) memorize_word(word.word);
                break;
            case 9: dictionary_stats(); break;
            case 10: vault_menu(); break;
            case 11: games_menu(); break;
            case 12: show_activity_results(); break;
            case 13: show_performance_statistics(); break;
            case 0: return;
            default: puts("Choose a number from the menu.");
        }
    }
}

void print_help(const char *program) {
    printf("WordMate - terminal dictionary and vocabulary assistant\n\nUsage: %s [command] [arguments]\n\n"
           "Commands:\n"
           "  search WORD                         Search and show meaning/synonyms\n"
           "  add WORD MEANING [SYNONYMS]         Add a dictionary entry\n"
           "  update WORD MEANING [SYNONYMS]      Replace an entry\n"
           "  delete WORD                         Delete all definitions\n"
           "  random                              Show a random word of the day\n"
           "  memorize WORD | memorized           Save or list memorized words\n"
           "  stats                               Show dictionary statistics\n"
           "  vault                               Open the password-protected Secret Vault\n"
           "  exam N [easy|medium|hard] [word|meaning|mixed]  Start an MCQ exam\n"
           "  guess-word | guess-meaning | word-search       Start a vocabulary game\n"
           "  history | performance              Show saved progress\n\n"
           "The interactive menu starts when no command is provided.\n", program);
}
