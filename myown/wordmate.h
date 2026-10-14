#ifndef WORDMATE_H
#define WORDMATE_H

#define WORD_MAX 128
#define MEANING_MAX 4096
#define SYNONYMS_MAX 512
#define LINE_MAX 8192
#define DATA_DIR "data"
#define DICTIONARY_DIR "data/dictionary"

typedef struct {
    char word[WORD_MAX];
    char meaning[MEANING_MAX];
    char synonyms[SYNONYMS_MAX];
} Word;

int ensure_data_ready(void);

int dictionary_search(const char *word);
int dictionary_add(const Word *word);
int dictionary_update(const Word *word);
int dictionary_delete(const char *word);

void normalize_word(char *word);
int word_file_path(const char *word, char *path,
                   unsigned long size);

void run_menu(void);

#endif