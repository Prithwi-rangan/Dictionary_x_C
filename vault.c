#include "vault_internal.h"

/* Each plaintext alphabet letter maps to the letter in the same cipher position. */
static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
static const char cipher_alphabet[] = "lpzqaokmjinxwsrvfdcehyubgt";

/* Apply the forward substitution when saving and the reverse mapping when reading. */
static void substitute(char *text, int decode) {
    for (char *p = text; *p; ++p) {
        int upper = isupper((unsigned char)*p);
        char lower = (char)tolower((unsigned char)*p);
        const char *from = decode ? cipher_alphabet : alphabet;
        const char *to = decode ? alphabet : cipher_alphabet;
        const char *position = strchr(from, lower);
        if (position) {
            char result = to[position - from];
            *p = upper ? (char)toupper((unsigned char)result) : result;
        }
    }
}

/* Encrypt a message in memory, append it to the fixed-size vault record, and save. */
int vault_add(const char *message) {
    VaultData data;
    if (!vault_require_unlocked()) return 0;
    if (!message || !message[0]) {
        puts("Enter a non-empty secret message.");
        return 0;
    }
    if (!vault_load_data(&data)) {
        puts("The vault file could not be read.");
        return 0;
    }
    if (data.message_count >= VAULT_MAX_MESSAGES) {
        printf("The vault can hold up to %d messages.\n", VAULT_MAX_MESSAGES);
        secure_clear(&data, sizeof(data));
        return 0;
    }
    snprintf(data.messages[data.message_count], sizeof(data.messages[0]), "%s", message);
    substitute(data.messages[data.message_count], 0);
    data.message_count++;
    int ok = vault_save_data(&data);
    secure_clear(&data, sizeof(data));
    if (ok) puts("Secret message saved.");
    return ok;
}

/* Copy each stored message before decoding so the encrypted record stays unchanged. */
int vault_list(void) {
    VaultData data;
    if (!vault_require_unlocked()) return 0;
    if (!vault_load_data(&data)) {
        puts("The vault file could not be read.");
        return 0;
    }
    if (data.message_count == 0) puts("The vault is empty.");
    for (unsigned int i = 0; i < data.message_count; ++i) {
        char message[MEANING_MAX];
        snprintf(message, sizeof(message), "%s", data.messages[i]);
        substitute(message, 1);
        printf("%u. %s\n", i + 1, message);
        secure_clear(message, sizeof(message));
    }
    secure_clear(&data, sizeof(data));
    return 1;
}

/* Remove one numbered item by shifting later encrypted messages down one slot. */
int vault_delete(size_t selected) {
    VaultData data;
    if (!vault_require_unlocked()) return 0;
    if (!vault_load_data(&data)) return 0;
    if (selected == 0 || selected > data.message_count) {
        puts("No message has that number.");
        secure_clear(&data, sizeof(data));
        return 0;
    }
    for (size_t i = selected - 1; i + 1 < data.message_count; ++i)
        memcpy(data.messages[i], data.messages[i + 1], sizeof(data.messages[i]));
    secure_clear(data.messages[data.message_count - 1], sizeof(data.messages[0]));
    data.message_count--;
    int ok = vault_save_data(&data);
    secure_clear(&data, sizeof(data));
    if (ok) puts("Message deleted.");
    return ok;
}

/* Read one line for the vault submenu. */
static int read_line(char *buffer, size_t size) {
    if (!fgets(buffer, (int)size, stdin)) return 0;
    trim_newline(buffer);
    return 1;
}

/* Keep vault prompts together here; authentication and file operations are separate. */
void vault_menu(void) {
    char input[LINE_MAX];
    for (;;) {
        printf("\n========== WORDMATE SECRET VAULT ==========\nStatus: %s\n"
               "1. Create vault\n2. Unlock vault\n3. Add secret message\n4. Read secret messages\n"
               "5. Delete a message\n6. Change password\n7. Reset entire vault\n0. Return to main menu\n",
               vault_is_unlocked() ? "UNLOCKED" : "LOCKED");
        fputs("Choose: ", stdout);
        if (!read_line(input, sizeof(input))) break;
        int choice = atoi(input);
        if (choice == 0) break;
        switch (choice) {
            case 1: vault_create(); break;
            case 2: vault_unlock(); break;
            case 3: {
                if (!vault_is_unlocked() && !vault_unlock()) break;
                char message[MEANING_MAX];
                fputs("Secret message: ", stdout);
                if (read_line(message, sizeof(message))) vault_add(message);
                secure_clear(message, sizeof(message));
                break;
            }
            case 4:
                if (!vault_is_unlocked() && !vault_unlock()) break;
                vault_list();
                break;
            case 5: {
                if (!vault_is_unlocked() && !vault_unlock()) break;
                vault_list();
                fputs("Message number to delete: ", stdout);
                if (read_line(input, sizeof(input))) {
                    int selected = atoi(input);
                    if (selected > 0) vault_delete((size_t)selected);
                    else puts("Enter a valid message number greater than zero.");
                }
                break;
            }
            case 6:
                if (!vault_is_unlocked() && !vault_unlock()) break;
                vault_change_password();
                break;
            case 7: vault_reset(); break;
            default: puts("Choose a number from the vault menu.");
        }
    }
    vault_lock();
}
