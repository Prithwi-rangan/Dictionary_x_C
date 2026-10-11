#ifndef WORDMATE_H
#define WORDMATE_H

#include <stddef.h>

#define WORD_MAX 128
#define MEANING_MAX 4096
#define SYNONYMS_MAX 512
#define GAME_MEANING_MAX 1024
#define LINE_MAX 8192
#define DATA_DIR "data"
#define DICTIONARY_DIR "data/dictionary"
#define MEMORIZED_FILE "data/memorized.txt"
#define VAULT_FILE "data/secrets.bin"
#define VAULT_META_FILE "data/vault.meta"
#define PASSWORD_MAX 256

typedef struct {
    char word[WORD_MAX];
    char meaning[MEANING_MAX];
    char synonyms[SYNONYMS_MAX];
} Word;

typedef struct {
    char word[WORD_MAX];
    char meaning[GAME_MEANING_MAX];
} GameWord;

typedef enum {
    OP_SEARCH, OP_ADD, OP_UPDATE, OP_DELETE, OP_RANDOM,
    OP_MEMORIZE, OP_STATS, OP_VAULT, OP_HELP
} Operation;

typedef struct {
    Operation operation;
    union {
        char word[WORD_MAX];
        char text[MEANING_MAX];
    } argument;
} Command;

int ensure_data_ready(void);
int dictionary_search(const char *word, int show_synonyms);
int dictionary_add(const Word *word);
int dictionary_update(const Word *word);
int dictionary_delete(const char *word);
int dictionary_suggest(const char *word);
int dictionary_random(void);
int dictionary_stats(void);
int dictionary_load_game_words(GameWord **words, size_t *count);
int memorize_word(const char *word);
int show_memorized(void);
int vault_is_configured(void);
int vault_is_unlocked(void);
int vault_create(void);
int vault_unlock(void);
void vault_lock(void);
int vault_add(const char *message);
int vault_list(void);
int vault_delete(size_t index);
int vault_change_password(void);
int vault_reset(void);
void vault_menu(void);
int save_activity_result(const char *activity, int correct, int total,
                         int points, unsigned long duration_seconds);
int show_activity_results(void);
int show_performance_statistics(void);
void games_menu(void);
int run_exam(int question_count, int difficulty, int question_mode);
int run_guess_word(void);
int run_guess_meaning(void);
int run_word_search(void);
void run_menu(void);
void print_help(const char *program);
void trim_newline(char *text);
void normalize_word(char *word);
int word_file_path(const char *word, char *path, size_t size);
int read_password(const char *prompt, char *buffer, size_t size);
void secure_clear(void *memory, size_t size);

#endif
