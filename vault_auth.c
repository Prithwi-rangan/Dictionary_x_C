#include "vault_internal.h"

static const char vault_magic[8] = {'W', 'M', 'S', 'E', 'C', 'R', 'T', '1'};
static int unlocked;

/* Simple FNV-style check for this learning exercise; it is not a secure password hash. */
static unsigned long password_hash(const char *password) {
    unsigned long hash = 2166136261UL;
    for (const unsigned char *p = (const unsigned char *)password; *p; ++p) {
        hash ^= *p;
        hash *= 16777619UL;
    }
    return hash;
}

/* Read and validate one complete binary record before using any of its fields. */
int vault_load_data(VaultData *data) {
    FILE *file;
    if (!data) return 0;
    file = fopen(VAULT_FILE, "rb");
    if (!file) return 0;
    int ok = fread(data, sizeof(*data), 1, file) == 1;
    int extra = fgetc(file);
    if (ferror(file) || extra != EOF) ok = 0;
    fclose(file);
    return ok && memcmp(data->magic, vault_magic, sizeof(vault_magic)) == 0 &&
           data->message_count <= VAULT_MAX_MESSAGES;
}

/* Write the whole fixed-size record so it can be read back with the same structure. */
int vault_save_data(const VaultData *data) {
    FILE *file;
    if (!data) return 0;
    file = fopen(VAULT_FILE, "wb");
    if (!file) {
        perror(VAULT_FILE);
        return 0;
    }
    int ok = fwrite(data, sizeof(*data), 1, file) == 1;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

/* Block message operations while the in-memory session flag says the vault is locked. */
int vault_require_unlocked(void) {
    if (!unlocked) puts("Unlock the vault first.");
    return unlocked;
}

/* Change only the session flag; the next program run starts locked again. */
void vault_set_unlocked(int state) {
    unlocked = state != 0;
}

/* Report whether this process has passed the password gate. */
int vault_is_unlocked(void) {
    return unlocked;
}

/* A vault is configured only when its binary record passes validation. */
int vault_is_configured(void) {
    VaultData data;
    int configured = vault_load_data(&data);
    secure_clear(&data, sizeof(data));
    return configured;
}

void vault_lock(void) {
    unlocked = 0;
}

/* Create a fresh record after the user enters and confirms a password. */
int vault_create(void) {
    VaultData data;
    char password[PASSWORD_MAX] = {0};
    char confirmation[PASSWORD_MAX] = {0};

    if (vault_is_configured()) {
        puts("A vault already exists. Unlock it first.");
        return 0;
    }
    if (!read_password("Create a vault password: ", password, sizeof(password)) || !password[0]) {
        puts("A password is required.");
        secure_clear(password, sizeof(password));
        return 0;
    }
    if (!read_password("Confirm the password: ", confirmation, sizeof(confirmation)) ||
        strcmp(password, confirmation) != 0) {
        puts("The passwords do not match.");
        secure_clear(password, sizeof(password));
        secure_clear(confirmation, sizeof(confirmation));
        return 0;
    }
    memset(&data, 0, sizeof(data));
    memcpy(data.magic, vault_magic, sizeof(vault_magic));
    data.password_hash = password_hash(password);
    int ok = vault_save_data(&data);
    secure_clear(password, sizeof(password));
    secure_clear(confirmation, sizeof(confirmation));
    secure_clear(&data, sizeof(data));
    if (ok) {
        unlocked = 1;
        puts("Vault created and unlocked for this session.");
    }
    return ok;
}

/* Allow five guesses; after the fifth wrong password, truncate the vault file. */
int vault_unlock(void) {
    VaultData data;
    char password[PASSWORD_MAX];
    if (!vault_load_data(&data)) {
        puts("Create a vault before unlocking it.");
        return 0;
    }
    for (int attempt = 1; attempt <= 5; ++attempt) {
        if (!read_password("Vault password: ", password, sizeof(password))) {
            secure_clear(password, sizeof(password));
            continue;
        }
        if (password_hash(password) == data.password_hash) {
            unlocked = 1;
            secure_clear(password, sizeof(password));
            secure_clear(&data, sizeof(data));
            puts("Vault unlocked for this session.");
            return 1;
        }
        secure_clear(password, sizeof(password));
        printf("Incorrect password (%d of 5 attempts).\n", attempt);
    }
    FILE *empty = fopen(VAULT_FILE, "wb");
    if (empty) fclose(empty);
    secure_clear(&data, sizeof(data));
    unlocked = 0;
    puts("Five incorrect attempts. The contents of secret.txt have been deleted.");
    return 0;
}

/* Verify the current password, replace its stored check, and keep messages intact. */
int vault_change_password(void) {
    VaultData data;
    char current[PASSWORD_MAX] = {0};
    char password[PASSWORD_MAX] = {0};
    char confirmation[PASSWORD_MAX] = {0};
    if (!vault_require_unlocked() || !vault_load_data(&data)) return 0;
    if (!read_password("Current password: ", current, sizeof(current)) ||
        password_hash(current) != data.password_hash) {
        puts("Password verification failed.");
        goto done;
    }
    if (!read_password("New password: ", password, sizeof(password)) || !password[0] ||
        !read_password("Confirm new password: ", confirmation, sizeof(confirmation)) ||
        strcmp(password, confirmation) != 0) {
        puts("The new passwords do not match or are empty.");
        goto done;
    }
    data.password_hash = password_hash(password);
    if (!vault_save_data(&data)) goto done;
    puts("Vault password changed.");
    secure_clear(current, sizeof(current));
    secure_clear(password, sizeof(password));
    secure_clear(confirmation, sizeof(confirmation));
    secure_clear(&data, sizeof(data));
    return 1;

done:
    secure_clear(current, sizeof(current));
    secure_clear(password, sizeof(password));
    secure_clear(confirmation, sizeof(confirmation));
    secure_clear(&data, sizeof(data));
    return 0;
}

/* Require an explicit RESET response before deleting the vault file. */
int vault_reset(void) {
    char confirmation[32];
    fputs("Reset deletes the vault and all messages. Type RESET to continue: ", stdout);
    if (!fgets(confirmation, sizeof(confirmation), stdin)) return 0;
    trim_newline(confirmation);
    if (strcmp(confirmation, "RESET") != 0) {
        puts("Reset cancelled.");
        return 0;
    }
    vault_lock();
    if (remove(VAULT_FILE) != 0) {
        puts("No vault file was removed.");
        return 0;
    }
    puts("Vault deleted. Create a new vault to use it again.");
    return 1;
}
