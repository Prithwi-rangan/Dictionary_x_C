#include "wordmate.h"

#include <ctype.h>
#include <time.h>

#define SEARCH_SIZE 10
#define SEARCH_WORDS 4

/* Remember where each hidden grid word was placed for checking the answer. */
typedef struct { int row, col, vertical; } Placement;

/* Read a game response from standard input. */
static int read_line(const char *prompt, char *buffer, size_t size) {
    fputs(prompt, stdout);
    if (!fgets(buffer, (int)size, stdin)) return 0;
    trim_newline(buffer);
    return 1;
}

/* Make a shuffled sequence of all pool indexes using Fisher-Yates swaps. */
static size_t *make_order(size_t count) {
    size_t *order = malloc(count * sizeof(*order));
    if (!order) return NULL;
    for (size_t i = 0; i < count; ++i) order[i] = i;
    for (size_t i = count; i > 1; --i) {
        size_t j = (size_t)rand() % i;
        size_t swap = order[i - 1]; order[i - 1] = order[j]; order[j] = swap;
    }
    return order;
}

/* Difficulty is based on headword length: short, unrestricted, or long. */
static int suitable_for_difficulty(const GameWord *word, int difficulty) {
    size_t length = strlen(word->word);
    if (difficulty == 0) return length <= 7;
    if (difficulty == 2) return length >= 8;
    return 1;
}

/* Filter by difficulty, fall back if too few words remain, then shuffle. */
static size_t *filtered_order(const GameWord *pool, size_t count, int difficulty, size_t *result_count) {
    size_t *order = malloc(count * sizeof(*order));
    if (!order) return NULL;
    size_t used = 0;
    for (size_t i = 0; i < count; ++i)
        if (suitable_for_difficulty(&pool[i], difficulty)) order[used++] = i;
    if (used < 4) {
        used = count;
        for (size_t i = 0; i < count; ++i) order[i] = i;
    }
    for (size_t i = used; i > 1; --i) {
        size_t j = (size_t)rand() % i;
        size_t swap = order[i - 1]; order[i - 1] = order[j]; order[j] = swap;
    }
    *result_count = used;
    return order;
}

/* Show one multiple-choice question with three distinct distractors. */
static int choose_four(const GameWord *pool, size_t count, size_t correct,
                       int correct_is_word, int difficulty) {
    if (count < 4) { puts("At least four suitable dictionary words are needed for multiple choice."); return 0; }
    size_t choices[4];
    choices[0] = correct;
    int used = 1;
    while (used < 4) {
        size_t candidate = (size_t)rand() % count;
        int duplicate = candidate == correct;
        for (int i = 0; i < used; ++i) if (choices[i] == candidate) duplicate = 1;
        if (!duplicate) choices[used++] = candidate;
    }
    /* Shuffle all four choices so the correct answer is not always option 1. */
    for (int i = 4; i > 1; --i) {
        int j = rand() % i;
        size_t swap = choices[i - 1]; choices[i - 1] = choices[j]; choices[j] = swap;
    }
    int answer = 0;
    for (int i = 0; i < 4; ++i) if (choices[i] == correct) answer = i + 1;
    if (correct_is_word) {
        printf("Meaning: %s\n", pool[correct].meaning);
        if (difficulty == 0) printf("Hint: starts with '%c'; %zu letters.\n", toupper((unsigned char)pool[correct].word[0]), strlen(pool[correct].word));
        else if (difficulty == 1) printf("Hint: %zu letters.\n", strlen(pool[correct].word));
    } else {
        printf("Word: %s\n", pool[correct].word);
        if (difficulty == 0) printf("Hint: meaning starts with '%c'.\n", toupper((unsigned char)pool[correct].meaning[0]));
    }
    for (int i = 0; i < 4; ++i) {
        const char *choice = correct_is_word ? pool[choices[i]].word : pool[choices[i]].meaning;
        printf("%d) %s\n", i + 1, choice);
    }
    char input[LINE_MAX];
    for (;;) {
        if (!read_line("Answer (1-4): ", input, sizeof(input))) return 0;
        char *end = NULL;
        long selected = strtol(input, &end, 10);
        if (end != input && !*end && selected >= 1 && selected <= 4) {
            int right = selected == answer;
            puts(right ? "Correct!" : "Not quite.");
            return right;
        }
        puts("Enter 1, 2, 3 or 4.");
    }
}

int run_exam(int question_count, int difficulty, int question_mode) {
    GameWord *pool = NULL;
    size_t count = 0;
    if (!dictionary_load_game_words(&pool, &count)) return 0;
    size_t available = 0;
    size_t *order = filtered_order(pool, count, difficulty, &available);
    if (!order) { free(pool); return 0; }
    if (question_count > (int)available) question_count = (int)available;
    if (question_count < 1) { free(order); free(pool); return 0; }
    printf("\nVocabulary test: %d questions, %s difficulty\n", question_count,
           difficulty == 0 ? "easy" : difficulty == 2 ? "hard" : "medium");
    time_t started = time(NULL);
    int correct = 0;
    /* Ask each selected word once; mixed mode randomly changes question direction. */
    for (int q = 0; q < question_count; ++q) {
        printf("\nQuestion %d of %d\n", q + 1, question_count);
        int mode = question_mode == 2 ? rand() % 2 : question_mode;
        /* Mode 0 asks for a meaning; mode 1 asks for the word. */
        correct += choose_four(pool, count, order[q], mode == 1, difficulty);
    }
    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    int points = correct * 10;
    printf("\n========== EXAM RESULT ==========\nQuestions: %d\nCorrect:   %d\nIncorrect: %d\nAccuracy:  %.2f%%\nPoints:    %d\nTime:      %02lu:%02lu\n",
           question_count, correct, question_count - correct,
           100.0 * correct / question_count, points, elapsed / 60, elapsed % 60);
    save_activity_result("MCQ Exam", correct, question_count, points, elapsed);
    free(order); free(pool);
    return 1;
}

/* Play short Hangman rounds: the meaning is the clue and each round gives five lives. */
int run_guess_word(void) {
    GameWord *pool = NULL; size_t count = 0;
    if (!dictionary_load_game_words(&pool, &count)) return 0;
    size_t available = 0; size_t *order = filtered_order(pool, count, 1, &available);
    if (!order) { free(pool); return 0; }
    int total = available < 10 ? (int)available : 10;
    int correct = 0, points = 0;
    time_t started = time(NULL);
    puts("\nGuess the Word: use the meaning clue and reveal letters by guessing.");
    for (int q = 0; q < total; ++q) {
        GameWord *item = &pool[order[q]];
        size_t length = strlen(item->word);
        char guessed[27] = {0};
        int lives = 5;
        int solved = 0;
        printf("\nRound %d of %d\nMeaning: %s\n", q + 1, total, item->meaning);
        /* Show guessed letters and mask every other position with an asterisk. */
        while (lives > 0 && !solved) {
            int revealed = 1;
            printf("Word: ");
            for (size_t i = 0; i < length; ++i) {
                char letter = item->word[i];
                if (letter >= 'a' && letter <= 'z' && guessed[letter - 'a']) putchar((char)toupper((unsigned char)letter));
                else { putchar('*'); revealed = 0; }
            }
            printf("   Lives: %d/5\n", lives);
            if (revealed) { solved = 1; break; }
            char guess[WORD_MAX];
            if (!read_line("Guess a letter or the whole word: ", guess, sizeof(guess))) break;
            normalize_word(guess);
            if (strlen(guess) == 1 && guess[0] >= 'a' && guess[0] <= 'z') {
                int letter = guess[0] - 'a';
                if (guessed[letter]) {
                    puts("You already tried that letter.");
                    continue;
                }
                guessed[letter] = 1;
                if (!strchr(item->word, guess[0])) {
                    lives--;
                    puts("That letter is not in the word.");
                } else puts("Good guess!");
            } else if (strcmp(guess, item->word) == 0) {
                solved = 1;
            } else {
                lives--;
                puts("That is not the word.");
            }
        }
        if (solved) {
            puts("Correct!");
            correct++;
            /* Award more points when the word is solved with more lives remaining. */
            points += lives * 2;
        } else printf("The word was '%s'.\n", item->word);
    }
    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    printf("\nGuess the Word score: %d/%d correct, %d points, %.2f%% accuracy.\n",
           correct, total, points, total ? 100.0 * correct / total : 0.0);
    save_activity_result("Guess the Word", correct, total, points, elapsed);
    free(order); free(pool); return 1;
}

/* Ask the player to identify the correct definition for each displayed word. */
int run_guess_meaning(void) {
    GameWord *pool = NULL; size_t count = 0;
    if (!dictionary_load_game_words(&pool, &count)) return 0;
    size_t available = 0; size_t *order = filtered_order(pool, count, 1, &available);
    if (!order) { free(pool); return 0; }
    int total = available < 10 ? (int)available : 10;
    int correct = 0;
    time_t started = time(NULL);
    puts("\nGuess the Meaning: choose the definition that matches each word.");
    for (int q = 0; q < total; ++q) {
        printf("\nRound %d of %d\n", q + 1, total);
        correct += choose_four(pool, count, order[q], 0, 1);
    }
    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    printf("\nGuess the Meaning score: %d/%d, %.2f%% accuracy.\n", correct, total,
           total ? 100.0 * correct / total : 0.0);
    save_activity_result("Guess the Meaning", correct, total, correct * 10, elapsed);
    free(order); free(pool); return 1;
}

/* Check that a word fits in the grid and only overlaps matching letters. */
static int can_place(char grid[SEARCH_SIZE][SEARCH_SIZE], const char *word,
                     int row, int col, int vertical) {
    size_t length = strlen(word);
    if ((vertical && row + (int)length > SEARCH_SIZE) ||
        (!vertical && col + (int)length > SEARCH_SIZE)) return 0;
    for (size_t i = 0; i < length; ++i) {
        int r = row + (vertical ? (int)i : 0), c = col + (vertical ? 0 : (int)i);
        char existing = grid[r][c];
        if (existing != '*' && existing != (char)toupper((unsigned char)word[i])) return 0;
    }
    return 1;
}

/* Try random horizontal or vertical positions, then save the successful location. */
static void place_word(char grid[SEARCH_SIZE][SEARCH_SIZE], const char *word,
                       Placement *place) {
    size_t length = strlen(word);
    for (int tries = 0; tries < 1000; ++tries) {
        int vertical = rand() % 2;
        int row = rand() % SEARCH_SIZE, col = rand() % SEARCH_SIZE;
        if (!can_place(grid, word, row, col, vertical)) continue;
        for (size_t i = 0; i < length; ++i) {
            int r = row + (vertical ? (int)i : 0), c = col + (vertical ? 0 : (int)i);
            grid[r][c] = (char)toupper((unsigned char)word[i]);
        }
        place->row = row; place->col = col; place->vertical = vertical;
        return;
    }
    place->row = place->col = -1;
}

/* Place a few words in a 10x10 grid, fill gaps with letters, and check coordinates. */
int run_word_search(void) {
    GameWord *pool = NULL; size_t count = 0;
    if (!dictionary_load_game_words(&pool, &count)) return 0;
    size_t *order = make_order(count);
    if (!order) { free(pool); return 0; }
    char grid[SEARCH_SIZE][SEARCH_SIZE];
    memset(grid, '*', sizeof(grid));
    size_t target_indices[SEARCH_WORDS];
    Placement placements[SEARCH_WORDS];
    int targets = 0;
    for (size_t i = 0; i < count && targets < SEARCH_WORDS; ++i) {
        size_t length = strlen(pool[order[i]].word);
        if (length < 3 || length > 8) continue;
        Placement place;
        place_word(grid, pool[order[i]].word, &place);
        if (place.row < 0) continue;
        target_indices[targets] = order[i];
        placements[targets++] = place;
    }
    free(order);
    if (targets < 2) { free(pool); puts("Not enough short words are available for the grid."); return 0; }
    /* Replace empty cells with random letters after all target words are placed. */
    for (int r = 0; r < SEARCH_SIZE; ++r)
        for (int c = 0; c < SEARCH_SIZE; ++c)
            if (grid[r][c] == '*') grid[r][c] = (char)('A' + rand() % 26);
    puts("\nFind these words in the 10x10 grid (horizontal or vertical):");
    for (int i = 0; i < targets; ++i) printf("%s%s", pool[target_indices[i]].word, i + 1 == targets ? "\n\n" : ", ");
    printf("     "); for (int c = 1; c <= SEARCH_SIZE; ++c) printf("%2d ", c); putchar('\n');
    for (int r = 0; r < SEARCH_SIZE; ++r) {
        printf("%2d   ", r + 1);
        for (int c = 0; c < SEARCH_SIZE; ++c) printf(" %c ", grid[r][c]);
        putchar('\n');
    }
    time_t started = time(NULL);
    int correct = 0;
    for (int i = 0; i < targets; ++i) {
        char prompt[128], input[LINE_MAX];
        snprintf(prompt, sizeof(prompt), "Locate %-16s as row column direction (H/V): ", pool[target_indices[i]].word);
        if (!read_line(prompt, input, sizeof(input))) break;
        int row = 0, col = 0; char direction = '\0';
        if (sscanf(input, "%d %d %c", &row, &col, &direction) == 3) {
            direction = (char)toupper((unsigned char)direction);
            if (row == placements[i].row + 1 && col == placements[i].col + 1 &&
                ((direction == 'V') == placements[i].vertical) && (direction == 'V' || direction == 'H')) {
                puts("Correct location!"); ++correct; continue;
            }
        }
        puts("That location does not match.");
    }
    unsigned long elapsed = (unsigned long)(time(NULL) - started);
    printf("\nWord Search score: %d/%d words found (%0.2f%%).\n", correct, targets,
           100.0 * correct / targets);
    save_activity_result("Word Search", correct, targets, correct * 10, elapsed);
    free(pool); return 1;
}

/* Collect exam settings and route each menu choice to its game or history view. */
void games_menu(void) {
    char input[LINE_MAX];
    for (;;) {
        puts("\n=== WordMate Learning Center ===\n1. Vocabulary examination (10/30/50 questions)\n2. Guess the Word (5 lives)\n3. Guess the Meaning\n4. 2D Word Search\n5. Activity history\n6. Performance summary\n0. Back");
        if (!read_line("Choose: ", input, sizeof(input))) return;
        int choice = atoi(input);
        if (choice == 0) return;
        if (choice == 1) {
            int number = 0, difficulty = 1, mode = 2;
            if (!read_line("Number of questions (10, 30, 50): ", input, sizeof(input))) return;
            number = atoi(input);
            if (number != 10 && number != 30 && number != 50) { puts("Choose 10, 30 or 50 questions."); continue; }
            if (!read_line("Difficulty (1 easy, 2 medium, 3 hard): ", input, sizeof(input))) return;
            difficulty = atoi(input) - 1;
            if (difficulty < 0 || difficulty > 2) { puts("Choose difficulty 1, 2 or 3."); continue; }
            if (!read_line("Question type (1 word-to-meaning, 2 meaning-to-word, 3 mixed): ", input, sizeof(input))) return;
            mode = atoi(input) - 1;
            if (mode < 0 || mode > 2) { puts("Choose question type 1, 2 or 3."); continue; }
            run_exam(number, difficulty, mode);
        } else if (choice == 2) run_guess_word();
        else if (choice == 3) run_guess_meaning();
        else if (choice == 4) run_word_search();
        else if (choice == 5) show_activity_results();
        else if (choice == 6) show_performance_statistics();
        else puts("Choose one of the listed activities.");
    }
}
