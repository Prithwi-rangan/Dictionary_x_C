#include "wordmate.h"

/* Remove either newline convention so prompts and file records compare cleanly. */
void trim_newline(char *text) {
    if (text) {
        text[strcspn(text, "\r\n")] = '\0';
    }
}

/* Read one fixed-size text line and discard any part that would overflow the buffer. */
int read_file_line(FILE *file, char *buffer, size_t size) {
    size_t length;
    int character;

    if (!file || !buffer || size == 0 || !fgets(buffer, (int)size, file)) {
        return 0;
    }
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] != '\n' && !feof(file)) {
        /* Consume the rest of an overlong record before the next read begins. */
        while ((character = fgetc(file)) != '\n' && character != EOF) {
            /* Discard the rest of an overlong line. */
        }
    }
    trim_newline(buffer);
    return 1;
}

/* Trim surrounding whitespace and use lowercase for consistent word comparisons. */
void normalize_word(char *word) {
    size_t length;
    size_t start = 0;

    if (!word) {
        return;
    }
    length = strlen(word);
    while (start < length && isspace((unsigned char)word[start])) {
        start++;
    }
    while (length > start && isspace((unsigned char)word[length - 1])) {
        length--;
    }
    /* Move the trimmed text to the front before adding its new terminator. */
    if (start > 0) {
        memmove(word, word + start, length - start);
    }
    word[length - start] = '\0';
    for (size_t i = 0; word[i]; ++i) {
        word[i] = (char)tolower((unsigned char)word[i]);
    }
}

/* A word is stored in the text file named for its first letter. */
int word_file_path(const char *word, char *path, size_t size) {
    int written;

    if (!word || !word[0] || !isalpha((unsigned char)word[0])) {
        return 0;
    }
    written = snprintf(path, size, DICTIONARY_DIR "/%c.txt",
                       tolower((unsigned char)word[0]));
    return written > 0 && (size_t)written < size;
}

/* Read a bounded password line; terminal input is visible in this simple version. */
int read_password(const char *prompt, char *buffer, size_t size) {
    size_t length;
    int character;

    if (!prompt || !buffer || size < 2) {
        return 0;
    }
    fputs(prompt, stdout);
    if (!fgets(buffer, (int)size, stdin)) {
        buffer[0] = '\0';
        return 0;
    }
    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] != '\n' && !feof(stdin)) {
        while ((character = getchar()) != '\n' && character != EOF) {
            /* Discard input that did not fit. */
        }
        buffer[0] = '\0';
        puts("Password is too long.");
        return 0;
    }
    trim_newline(buffer);
    return 1;
}

/* Overwrite a buffer through a volatile pointer so the clearing writes are kept. */
void secure_clear(void *memory, size_t size) {
    volatile unsigned char *bytes = (volatile unsigned char *)memory;
    while (size--) {
        *bytes++ = 0;
    }
}
