#include "wordmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_text(const char *prompt,
                     char *buffer,
                     size_t size)
{
    printf("%s", prompt);

    if (fgets(buffer, (int)size, stdin) == NULL)
        return 0;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    return 1;
}

static void read_new_word(Word *word, int ask_word)
{
    if (ask_word)
        read_text("Word: ", word->word, sizeof(word->word));

    read_text("Meaning: ",
              word->meaning, sizeof(word->meaning));

    read_text("Synonyms (optional): ",
              word->synonyms, sizeof(word->synonyms));
}

void run_menu(void)
{
    char input[32];
    char word[WORD_MAX];

    for (;;) {
        puts("\n=== Simple WordMate ===");
        puts("1. Search word");
        puts("2. Add word");
        puts("3. Update word");
        puts("4. Delete word");
        puts("0. Exit");

        if (!read_text("Choose: ", input, sizeof(input)))
            return;

        switch (atoi(input)) {
            case 1:
                if (read_text("Word to search: ",
                              word, sizeof(word))) {
                    dictionary_search(word);
                }
                break;

            case 2: {
                Word entry = {{0}, {0}, {0}};

                read_new_word(&entry, 1);
                dictionary_add(&entry);
                break;
            }

            case 3: {
                Word entry = {{0}, {0}, {0}};

                read_text("Word to update: ",
                          entry.word, sizeof(entry.word));

                read_new_word(&entry, 0);
                dictionary_update(&entry);
                break;
            }

            case 4:
                if (read_text("Word to delete: ",
                              word, sizeof(word))) {
                    dictionary_delete(word);
                }
                break;

            case 0:
                return;

            default:
                puts("Please choose 0, 1, 2, 3, or 4.");
        }
    }
}