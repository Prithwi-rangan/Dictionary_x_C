#include "wordmate.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void normalize_word(char *word)
{
    size_t i;
    size_t start = 0;
    size_t len;

    if (word == NULL)
        return;

    len = strlen(word);

    while (start < len && isspace((unsigned char)word[start]))
        start++;

    while (len > start &&
           isspace((unsigned char)word[len - 1]))
        len--;

    if (start > 0)
        memmove(word, word + start, len - start);

    word[len - start] = '\0';

    for (i = 0; word[i] != '\0'; i++)
        word[i] = (char)tolower((unsigned char)word[i]);
}

int word_file_path(const char *word,
                   char *path,
                   unsigned long size)
{
    int n;

    if (word == NULL || word[0] == '\0' ||
        !isalpha((unsigned char)word[0])) {
        return 0;
    }

    n = snprintf(path, size, DICTIONARY_DIR "/%c.txt",
                 tolower((unsigned char)word[0]));

    return n > 0 && (unsigned long)n < size;
}