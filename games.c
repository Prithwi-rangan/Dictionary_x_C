#include "wordmate.h"

#include <time.h>

static const char *const hangman_stages[] = {
    "  +---+\n  |   |\n  O   |\n /|\\  |\n / \\  |\n      |\n=========\n",
    "  +---+\n  |   |\n  O   |\n /|\\  |\n /    |\n      |\n=========\n",
    "  +---+\n  |   |\n  O   |\n /|\\  |\n      |\n      |\n=========\n",
    "  +---+\n  |   |\n  O   |\n /|   |\n      |\n      |\n=========\n",
    "  +---+\n  |   |\n  O   |\n  |   |\n      |\n      |\n=========\n",
    "  +---+\n  |   |\n  O   |\n      |\n      |\n      |\n=========\n",
    "  +---+\n  |   |\n      |\n      |\n      |\n      |\n=========\n"
};

static int read_line(const char *prompt, char *buffer, size_t size) {
    fputs(prompt, stdout);
    if (!fgets(buffer, (int)size, stdin)) return 0;
    trim_newline(buffer);
    return 1;
}

static int ask_meaning_question(const GameWord *pool, size_t count, size_t answer) {
    size_t choices[4];

    if (count < 4) {
        puts("At least four dictionary words are needed for the exam.");
        return -1;
    }

    choices[0] = answer;
    for (int i = 1; i < 4; ++i) {
        int duplicate;
        do {
            choices[i] = (size_t)rand() % count;
            duplicate = choices[i] == answer;
            for (int j = 1; j < i; ++j)
                if (choices[j] == choices[i]) duplicate = 1;
        } while (duplicate);
    }

    for (int i = 3; i > 0; --i) {
        int j = rand() % (i + 1);
        size_t swap = choices[i];
        choices[i] = choices[j];
        choices[j] = swap;
    }

    printf("Word: %s\n", pool[answer].word);
    for (int i = 0; i < 4; ++i)
        printf("%d) %s\n", i + 1, pool[choices[i]].meaning);

    char input[LINE_MAX];
    for (;;) {
        if (!read_line("Choose the meaning (1-4): ", input, sizeof(input))) return -1;
        char *end = NULL;
        long selected = strtol(input, &end, 10);
        if (end != input && *end == '\0' && selected >= 1 && selected <= 4) {
            int correct = choices[selected - 1] == answer;
            puts(correct ? "Correct!" : "Not quite.");
            return correct;
        }
        puts("Please enter 1, 2, 3, or 4.");
    }
}

int run_exam(int question_count) {
    GameWord *pool = NULL;
    size_t count = 0;

    if (question_count != 10 && question_count != 30) {
        puts("Choose 10 or 30 questions.");
        return 0;
    }
    if (!dictionary_load_game_words(&pool, &count)) return 0;
    if (count < 4) {
        puts("At least four dictionary words are needed for the exam.");
        free(pool);
        return 0;
    }

    printf("\nVocabulary exam: %d questions\n", question_count);
    time_t started = time(NULL);
    int correct = 0;
    for (int question = 1; question <= question_count; ++question) {
        printf("\nQuestion %d of %d\n", question, question_count);
        int result = ask_meaning_question(pool, count, (size_t)rand() % count);
        if (result < 0) {
            free(pool);
            return 0;
        }
        correct += result;
    }

    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    int points = correct * 10;
    printf("\nExam score: %d/%d correct (%0.2f%%).\n", correct, question_count,
           100.0 * correct / question_count);
    save_activity_result("Vocabulary Exam", correct, question_count, points, elapsed);
    free(pool);
    return 1;
}

static int is_simple_word(const char *word) {
    if (*word == '\0') return 0;
    for (const unsigned char *letter = (const unsigned char *)word; *letter; ++letter)
        if (*letter < 'a' || *letter > 'z') return 0;
    return 1;
}

int run_hangman(void) {
    GameWord *pool = NULL;
    size_t count = 0;
    size_t eligible = 0;

    if (!dictionary_load_game_words(&pool, &count)) return 0;
    for (size_t i = 0; i < count; ++i)
        if (is_simple_word(pool[i].word)) ++eligible;
    if (eligible == 0) {
        puts("Add an alphabetic word before playing Project Hangman.");
        free(pool);
        return 0;
    }

    size_t selected = (size_t)rand() % eligible;
    GameWord *answer = NULL;
    for (size_t i = 0; i < count; ++i) {
        if (is_simple_word(pool[i].word)) {
            if (selected == 0) {
                answer = &pool[i];
                break;
            }
            --selected;
        }
    }

    int guessed[26] = {0};
    int wrong_guesses = 0;
    int won = 0;
    time_t started = time(NULL);

    puts("\nProject Hangman");
    puts("Guess one letter at a time. You have 6 wrong guesses.");
    printf("Clue: %s\n", answer->meaning);
    fputs(hangman_stages[6], stdout);

    while (wrong_guesses < 6) {
        int revealed = 1;
        printf("Word: ");
        for (const char *letter = answer->word; *letter; ++letter) {
            if (guessed[*letter - 'a']) putchar((char)toupper((unsigned char)*letter));
            else {
                putchar('_');
                revealed = 0;
            }
        }
        putchar('\n');
        if (revealed) {
            won = 1;
            break;
        }

        char input[LINE_MAX];
        if (!read_line("Guess one letter: ", input, sizeof(input))) {
            free(pool);
            return 0;
        }
        if (strlen(input) != 1) {
            puts("Please enter one letter.");
            continue;
        }

        char guess = (char)tolower((unsigned char)input[0]);
        if (guess < 'a' || guess > 'z') {
            puts("Please enter an English letter.");
            continue;
        }
        if (guessed[guess - 'a']) {
            puts("You already tried that letter.");
            continue;
        }
        guessed[guess - 'a'] = 1;

        if (strchr(answer->word, guess)) puts("Good guess!");
        else {
            ++wrong_guesses;
            puts("That letter is not in the word.");
            fputs(hangman_stages[6 - wrong_guesses], stdout);
        }
    }

    if (won) printf("You win! The word was '%s'.\n", answer->word);
    else printf("Game over. The word was '%s'.\n", answer->word);

    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    save_activity_result("Project Hangman", won ? 1 : 0, 1, won ? 10 : 0, elapsed);
    free(pool);
    return 1;
}

/* Run one games-menu action, then let the main menu handle the continue prompt. */
int games_menu(void) {
    char input[LINE_MAX];

    puts("\n=== Games and exams ===");
    puts("1. Vocabulary exam (10 or 30 questions)");
    puts("2. Project Hangman");
    puts("3. Activity history and performance");
    puts("0. Back to the main menu");

    if (!read_line("Choose: ", input, sizeof(input))) return 0;

    char *end;
    long selected = strtol(input, &end, 10);
    while (*end && isspace((unsigned char)*end)) ++end;
    if (end == input || *end != '\0' || selected < 0 || selected > 3) {
        puts("Choose one of the listed options.");
        return 1;
    }
    int choice = (int)selected;
    if (choice == 0) return 0;

    if (choice == 1) {
        if (!read_line("Number of questions (10 or 30): ", input, sizeof(input))) {
            return 1;
        }
        int questions = atoi(input);
        if (questions != 10 && questions != 30) {
            puts("Please choose 10 or 30 questions.");
            return 1;
        }
        run_exam(questions);
    } else if (choice == 2) {
        run_hangman();
    } else if (choice == 3) {
        show_activity_summary();
    } else {
        puts("Choose one of the listed options.");
    }

    return 1;
}
