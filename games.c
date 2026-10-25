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

static int is_simple_word(const char *word) {
    if (*word == '\0') return 0;
    for (const unsigned char *letter = (const unsigned char *)word; *letter; ++letter)
        if (*letter < 'a' || *letter > 'z') return 0;
    return 1;
}

/* Choose a random game word, keeping Beginner words short and familiar. */
static GameWord *choose_random_word(GameWord *pool, size_t count, size_t max_length) {
    size_t available = 0;

    for (size_t i = 0; i < count; ++i) {
        size_t length = strlen(pool[i].word);
        if (is_simple_word(pool[i].word) && length <= max_length) available++;
    }
    if (available == 0) return NULL;

    size_t selected = (size_t)rand() % available;
    for (size_t i = 0; i < count; ++i) {
        size_t length = strlen(pool[i].word);
        if (!is_simple_word(pool[i].word) || length > max_length) continue;
        if (selected == 0) return &pool[i];
        --selected;
    }
    return NULL;
}

/* Guess a word from its meaning and a pattern with randomly hidden letters. */
int run_missing_letters(int advanced) {
    GameWord *pool = NULL;
    size_t count = 0;
    size_t max_length = advanced ? WORD_MAX - 1 : 8;

    if (!dictionary_load_game_words(&pool, &count)) return 0;
    GameWord *answer = choose_random_word(pool, count, max_length);
    if (!answer) {
        puts("There are no suitable words for this mode.");
        free(pool);
        return 0;
    }

    size_t length = strlen(answer->word);
    size_t missing_count = advanced ? (length + 1) / 2 : 1;
    size_t positions[WORD_MAX];
    int hidden[WORD_MAX] = {0};
    char pattern[WORD_MAX];

    /* Shuffle letter positions so each game hides a different set. */
    for (size_t i = 0; i < length; ++i) positions[i] = i;
    for (size_t i = length; i > 1; --i) {
        size_t j = (size_t)rand() % i;
        size_t swap = positions[i - 1];
        positions[i - 1] = positions[j];
        positions[j] = swap;
    }
    for (size_t i = 0; i < missing_count; ++i) hidden[positions[i]] = 1;

    /* Build the displayed pattern, using underscores for hidden letters. */
    for (size_t i = 0; i < length; ++i) {
        pattern[i] = hidden[i] ? '_' : answer->word[i];
    }
    pattern[length] = '\0';

    time_t started = time(NULL);
    int chances = 5;
    int won = 0;
    printf("\nMissing Letters - %s\n", advanced ? "Advanced" : "Beginner");
    printf("Hint: %s\n", answer->meaning);
    printf("Word: %s\n", pattern);
    puts("Guess the full word. You have 5 chances.");

    while (chances > 0) {
        char guess[WORD_MAX];
        if (!read_line("Your answer: ", guess, sizeof(guess))) {
            free(pool);
            return 0;
        }
        normalize_word(guess);
        if (!guess[0]) {
            puts("Please type a word.");
            continue;
        }
        if (strcmp(guess, answer->word) == 0) {
            won = 1;
            break;
        }

        --chances;
        if (chances > 0) {
            printf("Not quite. You have %d %s left.\n", chances,
                   chances == 1 ? "chance" : "chances");
        }
    }

    if (won) printf("Correct! The word is '%s'.\n", answer->word);
    else printf("No more chances. The word was '%s'.\n", answer->word);

    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    save_activity_result(advanced ? "Missing Letters Advanced" : "Missing Letters Beginner",
                         won ? 1 : 0, 1, won ? 10 : 0, elapsed);
    free(pool);
    return 1;
}

int run_hangman(void) {
    GameWord *pool = NULL;
    size_t count = 0;
    if (!dictionary_load_game_words(&pool, &count)) return 0;
    GameWord *answer = choose_random_word(pool, count, WORD_MAX - 1);
    if (!answer) {
        puts("There are no suitable words for Project Hangman.");
        free(pool);
        return 0;
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

    puts("\n=== Vocabulary Games ===");
    puts("1. Missing Letters - Beginner");
    puts("2. Missing Letters - Advanced");
    puts("3. Project Hangman");
    puts("4. Activity history");
    puts("0. Back to the main menu");

    if (!read_line("Choose: ", input, sizeof(input))) return 0;

    char *end;
    long selected = strtol(input, &end, 10);
    while (*end && isspace((unsigned char)*end)) ++end;
    if (end == input || *end != '\0' || selected < 0 || selected > 4) {
        puts("Choose one of the listed options.");
        return 1;
    }
    int choice = (int)selected;
    if (choice == 0) return 0;

    if (choice == 1) {
        run_missing_letters(0);
    } else if (choice == 2) {
        run_missing_letters(1);
    } else if (choice == 3) {
        run_hangman();
    } else if (choice == 4) {
        show_activity_history();
    } else {
        puts("Choose one of the listed options.");
    }

    return 1;
}
