#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define FILE_NAME "words.txt"
#define TEMP_FILE "temp.txt"

#define ENTRY_SIZE 8192

// Function declarations
void searchWord(void);
void addWord(void);
void deleteWord(void);
void updateWord(void);

int main(void)
{
    int choice;
    int count = 0;
 
    FILE *fp = fopen(FILE_NAME, "r");
    if (fp != NULL) {
        char line[2048];
        while (fgets(line, sizeof(line), fp))
            count++;
        fclose(fp);
    }
    
    printf("Total words in dictionary: %d\n", count);
        
    while (1)
    {
        printf("\n===== WORD MANAGER =====\n");
        printf("1. Search Word\n");
        printf("2. Add Word\n");
        printf("3. Delete Word\n");
        printf("4. Update Word\n");
        printf("5. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1){
            printf("Invalid input.\n");
            while (getchar() != '\n'); // Clear bad input buffer
            continue;
        }

        getchar();  // Remove newline after choice

        switch (choice)
        {
            case 1:
                searchWord();
                break;

            case 2:
                addWord();
                break;

            case 3:
                deleteWord();
                break;

            case 4:
                updateWord();
                break;

            case 5:
                printf("Goodbye!\n");
                return 0;

            default:
                printf("Invalid choice!\n");
        }
    }
}

void searchWord(void)
{
    char word[100];
    char line[2048];
    char firstWord[100];

    printf("Enter word to search: ");
    fgets(word, sizeof(word), stdin);
    word[strcspn(word, "\n")] = '\0';

    FILE *fp = fopen(FILE_NAME, "r");

    if (fp == NULL) {
        printf("Cannot open file! (Dictionary is empty)\n");
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL){
        if (sscanf(line, "%99s", firstWord) == 1){
            if (strcmp(firstWord, word) == 0){
                printf("%s", line);
                fclose(fp);
                return;
            }
        }
    }

    printf("Word not found!\n");
    fclose(fp);
}

// 2. ADD (Maintains Alphabetical Order & Duplicate Checking)
void addWord(void)
{
    char word[100];
    char line[2048];
    char firstWord[100];
    char definition[1500];
    int inserted = 0;

    printf("Enter new word: ");
    if (fgets(word, sizeof(word), stdin) == NULL)
        return;

    word[strcspn(word, "\n")] = '\0';

    if (word[0] == '\0') {
        printf("Word cannot be empty!\n");
        return;
    }

    // Get definition from user first
    printf("Enter definition for '%s': ", word);
    if (fgets(definition, sizeof(definition), stdin) == NULL)
        return;
    definition[strcspn(definition, "\n")] = '\0';

    FILE *fp = fopen(FILE_NAME, "r");
    FILE *temp = fopen(TEMP_FILE, "w");

    if (temp == NULL) {
        perror("Cannot create temporary file");
        if (fp) fclose(fp);
        return;
    }

    // If file doesn't exist yet, just write the new word
    if (fp == NULL) {
        fprintf(temp, "%s - %s\n", word, definition);
        fclose(temp);
        rename(TEMP_FILE, FILE_NAME);
        printf("Word added successfully!\n");
        return;
    }

    // Read through file, check duplicates, and insert in alphabetical order
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%99s", firstWord) == 1) {
            // Check for duplicate
            if (strcmp(firstWord, word) == 0) {
                printf("Word already exists! You can update or delete it.\n");
                fclose(fp);
                fclose(temp);
                remove(TEMP_FILE);
                return;
            }

            // Insert alphabetically if not yet inserted and current word comes after new word
            if (!inserted && strcmp(word, firstWord) < 0) {
                fprintf(temp, "%s - %s\n", word, definition);
                inserted = 1;
            }
        }
        fputs(line, temp);
    }

    // If it belongs at the very end of the file
    if (!inserted) {
        fprintf(temp, "%s - %s\n", word, definition);
    }

    fclose(fp);
    fclose(temp);

    remove(FILE_NAME);
    rename(TEMP_FILE, FILE_NAME);
    printf("Word added successfully!\n");
}

// 3. DELETE
void deleteWord(void)
{
    char word[100];
    char line[2048];
    char firstWord[100];
    int found = 0;

    printf("Enter word to delete: ");
    if (fgets(word, sizeof(word), stdin) == NULL)
        return;
    word[strcspn(word, "\n")] = '\0';

    if (word[0] == '\0') {
        printf("Word cannot be empty!\n");
        return;
    }

    FILE *fp = fopen(FILE_NAME, "r");
    if (fp == NULL) {
        printf("Cannot open file! Dictionary is empty.\n");
        return;
    }

    FILE *temp = fopen(TEMP_FILE, "w");
    if (temp == NULL) {
        perror("Cannot create temporary file");
        fclose(fp);
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%99s", firstWord) == 1 && strcmp(firstWord, word) == 0) {
            found = 1; // Skip writing this line to delete it
        } else {
            fputs(line, temp);
        }
    }

    fclose(fp);
    fclose(temp);

    if (found) {
        remove(FILE_NAME);
        rename(TEMP_FILE, FILE_NAME);
        printf("Word deleted successfully!\n");
    } else {
        remove(TEMP_FILE);
        printf("Word not found in dictionary!\n");
    }
}

// 4. UPDATE
void updateWord(void)
{
    char word[100];
    char line[2048];
    char firstWord[100];
    char newDefinition[1500];
    int found = 0;

    printf("Enter word to update: ");
    if (fgets(word, sizeof(word), stdin) == NULL)
        return;
    word[strcspn(word, "\n")] = '\0';

    if (word[0] == '\0') {
        printf("Word cannot be empty!\n");
        return;
    }

    FILE *fp = fopen(FILE_NAME, "r");
    if (fp == NULL) {
        printf("Cannot open file! Dictionary is empty.\n");
        return;
    }

    FILE *temp = fopen(TEMP_FILE, "w");
    if (temp == NULL) {
        perror("Cannot create temporary file");
        fclose(fp);
        return;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%99s", firstWord) == 1 && strcmp(firstWord, word) == 0) {
            found = 1;
            printf("Enter new definition for '%s': ", word);
            fgets(newDefinition, sizeof(newDefinition), stdin);
            newDefinition[strcspn(newDefinition, "\n")] = '\0';

            fprintf(temp, "%s - %s\n", word, newDefinition);
        } else {
            fputs(line, temp);
        }
    }

    fclose(fp);
    fclose(temp);

    if (found) {
        remove(FILE_NAME);
        rename(TEMP_FILE, FILE_NAME);
        printf("Word updated successfully!\n");
    } else {
        remove(TEMP_FILE);
        printf("Word not found in dictionary!\n");
    }
}