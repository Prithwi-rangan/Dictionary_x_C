#ifndef WORDMATE_H
#define WORDMATE_H

/* Shared declarations used by the menu, dictionary, learning, and games. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Fixed buffer sizes keep input and stored records simple for this project. */
#define WORD_MAX 128
#define MEANING_MAX 4096
#define GAME_MEANING_MAX 1024
#define LINE_MAX 8192
#define DATA_DIR "data"
#define DICTIONARY_DIR "data/dictionary"
#define MEMORIZED_FILE "data/memorized.txt"

/* A dictionary entry contains a headword and its meaning. */
typedef struct {
    char word[WORD_MAX];
    char meaning[MEANING_MAX];
} Word;

/* Games use shorter meaning buffers to keep their question pool compact. */
typedef struct {
    char word[WORD_MAX];
    char meaning[GAME_MEANING_MAX];
} GameWord;

/* Dictionary setup and lookup. */
int ensure_data_ready(void);
int dictionary_search(const char *word);
int dictionary_add(const Word *word);
int dictionary_update(const Word *word);
int dictionary_delete(const char *word);
int dictionary_random(void);
int dictionary_stats(void);
int dictionary_load_game_words(GameWord **words, size_t *count);
/* Memorized word list. */
int memorize_word(const char *word);
int show_memorized(void);
/* Activity history and learning games. */
int save_activity_result(const char *activity, int correct, int total,
                         int points, unsigned long duration_seconds);
int show_activity_history(void);
int games_menu(void);
int run_exam(int question_count);
int run_hangman(void);
void run_menu(void);
void print_help(const char *program);
/* Shared input and text helpers. */
void trim_newline(char *text);
int read_file_line(FILE *file, char *buffer, size_t size);
void normalize_word(char *word);
int word_file_path(const char *word, char *path, size_t size);

#endif
