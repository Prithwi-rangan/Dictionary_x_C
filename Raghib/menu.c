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
    int choice,count;
 
    FILE *fp = fopen(FILE_NAME, "r");
    char line[2048];
    while (fgets(line, sizeof(line), fp))
        count++;
    
    printf("%d",count);
        
        
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
    printf("%s",word);
    // Remove newline from user input
    word[strcspn(word, "\n")] = '\0';

    FILE *fp = fopen(FILE_NAME, "r");

    if (fp == NULL) {
        printf("Cannot open file!\n");
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


void addWord(void){
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

    FILE *fp = fopen(FILE_NAME, "r");

    if (fp == NULL) {
        perror("Cannot open dictionary");
        return;
    }

    /* PASS 1: Check for duplicate */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%99s", firstWord) == 1 &&
            strcmp(firstWord, word) == 0) {
            printf("Word already exists! "
                   "You can update or delete it.\n");
            fclose(fp);
            return;
        }
    }
	


}

// 3. DELETE
void deleteWord(void)
{
    char word[100];

    printf("Enter word to delete: ");
    fgets(word, sizeof(word), stdin);

}

// 4. UPDATE
void updateWord(void)
{
    char word[100];
    char definition[1500];

}