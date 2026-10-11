#include "wordmate.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void trim_newline(char *text) {
    if (!text) return;
    text[strcspn(text, "\r\n")] = '\0';
}

void normalize_word(char *word) {
    if (!word) return;
    while (isspace((unsigned char)*word)) memmove(word, word + 1, strlen(word));
    size_t length = strlen(word);
    while (length && isspace((unsigned char)word[length - 1])) word[--length] = '\0';
    for (size_t i = 0; word[i]; ++i) word[i] = (char)tolower((unsigned char)word[i]);
}

int word_file_path(const char *word, char *path, size_t size) {
    if (!word || !word[0] || !isalpha((unsigned char)word[0])) return 0;
    int n = snprintf(path, size, DICTIONARY_DIR "/%c.txt", tolower((unsigned char)word[0]));
    return n > 0 && (size_t)n < size;
}

int read_password(const char *prompt, char *buffer, size_t size) {
    if (!buffer || size < 2) return 0;
    fputs(prompt, stdout);
    fflush(stdout);
    struct termios original, hidden;
    int hide_input = 0;
    if (isatty(STDIN_FILENO)) {
        if (tcgetattr(STDIN_FILENO, &original) != 0) {
            fputs("Could not configure hidden password input.\n", stderr);
            return 0;
        }
        hidden = original;
        hidden.c_lflag &= (tcflag_t)~ECHO;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden) != 0) {
            fputs("Could not hide password input.\n", stderr);
            return 0;
        }
        hide_input = 1;
    }
    char *read = fgets(buffer, (int)size, stdin);
    if (hide_input) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        putchar('\n');
    }
    if (!read) { buffer[0] = '\0'; return 0; }
    size_t length = strlen(buffer);
    if (length && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
        if (length && buffer[length - 1] == '\r') buffer[--length] = '\0';
    } else if (!feof(stdin)) {
        int ch = getchar();
        if (ch != '\n' && ch != EOF) {
            while (ch != '\n' && ch != EOF) ch = getchar();
            buffer[0] = '\0';
            fputs("Password input is too long.\n", stderr);
            return 0;
        }
    }
    return 1;
}

void secure_clear(void *memory, size_t size) {
    volatile unsigned char *bytes = (volatile unsigned char *)memory;
    while (size--) *bytes++ = 0;
}
