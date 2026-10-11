#define _POSIX_C_SOURCE 200809L
#include "vault_internal.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * This is deliberately a small, dependency-free learning implementation.
 * Its password mixer and key wrapping are NOT cryptographic protection.
 */
static const unsigned char vault_magic[8] = {'W','M','V','A','U','L','T','1'};
static const uint64_t password_domain = UINT64_C(0x574d2d5041535321);
static const uint64_t wrap_domain = UINT64_C(0x574d2d5752415021);
static const uint64_t key_domain = UINT64_C(0x574d2d4b45592121);
static uint64_t active_vault_key;
static int active;

static uint64_t mix64(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static uint64_t derive_password_value(const char *password, uint64_t salt) {
    uint64_t value = UINT64_C(14695981039346656037) ^ salt;
    for (const unsigned char *p = (const unsigned char *)password; *p; ++p) {
        value ^= *p;
        value *= UINT64_C(1099511628211);
    }
    /* Intentionally educational, not a replacement for a password KDF. */
    for (uint32_t i = 0; i < 100000u; ++i) {
        value ^= salt + (uint64_t)i;
        value *= UINT64_C(1099511628211);
        value = mix64(value);
    }
    return value;
}

static void put_u32(unsigned char *out, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out[i] = (unsigned char)(value >> (8u * i));
}

static void put_u64(unsigned char *out, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out[i] = (unsigned char)(value >> (8u * i));
}

static uint32_t get_u32(const unsigned char *in) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= (uint32_t)in[i] << (8u * i);
    return value;
}

static uint64_t get_u64(const unsigned char *in) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= (uint64_t)in[i] << (8u * i);
    return value;
}

static void encode_meta(const VaultMeta *meta, unsigned char bytes[44]) {
    memcpy(bytes, meta->magic, 8);
    put_u32(bytes + 8, meta->version);
    put_u64(bytes + 12, meta->salt);
    put_u64(bytes + 20, meta->password_check);
    put_u64(bytes + 28, meta->wrapped_key);
    put_u64(bytes + 36, meta->key_check);
}

static void decode_meta(VaultMeta *meta, const unsigned char bytes[44]) {
    memset(meta, 0, sizeof(*meta));
    memcpy(meta->magic, bytes, 8);
    meta->version = get_u32(bytes + 8);
    meta->salt = get_u64(bytes + 12);
    meta->password_check = get_u64(bytes + 20);
    meta->wrapped_key = get_u64(bytes + 28);
    meta->key_check = get_u64(bytes + 36);
}

static int path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

int vault_random_bytes(void *buffer, size_t length) {
    FILE *source;
    if (!buffer || length == 0) return length == 0;
    source = fopen("/dev/urandom", "rb");
    if (!source) {
        fputs("Could not open the operating system random source (/dev/urandom).\n", stderr);
        memset(buffer, 0, length);
        return 0;
    }
    int ok = fread(buffer, 1, length, source) == length;
    if (fclose(source) != 0) ok = 0;
    if (!ok) {
        fputs("Could not read random bytes from the operating system.\n", stderr);
        memset(buffer, 0, length);
    }
    return ok;
}

static int load_meta(VaultMeta *meta) {
    unsigned char bytes[44];
    FILE *file = fopen(VAULT_META_FILE, "rb");
    if (!file) return 0;
    int ok = fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
    int extra = fgetc(file);
    if (ferror(file) || extra != EOF) ok = 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) return 0;
    decode_meta(meta, bytes);
    secure_clear(bytes, sizeof(bytes));
    return memcmp(meta->magic, vault_magic, sizeof(vault_magic)) == 0 &&
           meta->version == VAULT_FORMAT_VERSION;
}

static int write_meta_file(const char *path, const VaultMeta *meta) {
    unsigned char bytes[44];
    encode_meta(meta, bytes);
    FILE *file = fopen(path, "wb");
    if (!file) { perror(path); secure_clear(bytes, sizeof(bytes)); return 0; }
    int ok = fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes) &&
             fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file) != 0) ok = 0;
    if (ok) (void)chmod(path, S_IRUSR | S_IWUSR);
    secure_clear(bytes, sizeof(bytes));
    if (!ok) remove(path);
    return ok;
}

static int write_empty_secret_file(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) { perror(path); return 0; }
    int ok = fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) remove(path);
    return ok;
}

static int initialize_meta(VaultMeta *meta, const char *password, uint64_t *key) {
    memset(meta, 0, sizeof(*meta));
    memcpy(meta->magic, vault_magic, sizeof(vault_magic));
    meta->version = VAULT_FORMAT_VERSION;
    if (!vault_random_bytes(&meta->salt, sizeof(meta->salt)) ||
        !vault_random_bytes(key, sizeof(*key))) return 0;
    uint64_t derived = derive_password_value(password, meta->salt);
    meta->password_check = mix64(derived ^ password_domain);
    meta->wrapped_key = *key ^ mix64(derived ^ wrap_domain);
    meta->key_check = mix64(*key ^ key_domain);
    secure_clear(&derived, sizeof(derived));
    return 1;
}

static int prompt_new_password(char password[PASSWORD_MAX]) {
    char confirmation[PASSWORD_MAX] = {0};
    int ok = read_password("Create a vault password (at least 8 characters): ",
                           password, PASSWORD_MAX);
    if (ok && strlen(password) < 8) {
        puts("Use a password with at least 8 characters.");
        ok = 0;
    }
    if (ok) ok = read_password("Confirm new password: ", confirmation, sizeof(confirmation));
    if (ok && strcmp(password, confirmation) != 0) {
        puts("The passwords do not match.");
        ok = 0;
    }
    secure_clear(confirmation, sizeof(confirmation));
    if (!ok) secure_clear(password, PASSWORD_MAX);
    return ok;
}

static int unlock_with_password(const VaultMeta *meta, const char *password, uint64_t *key) {
    uint64_t derived = derive_password_value(password, meta->salt);
    uint64_t password_check = mix64(derived ^ password_domain);
    uint64_t candidate = meta->wrapped_key ^ mix64(derived ^ wrap_domain);
    uint64_t key_check = mix64(candidate ^ key_domain);
    int ok = (password_check ^ meta->password_check) == 0 &&
             (key_check ^ meta->key_check) == 0;
    if (ok) *key = candidate;
    else *key = 0;
    secure_clear(&derived, sizeof(derived));
    secure_clear(&candidate, sizeof(candidate));
    secure_clear(&password_check, sizeof(password_check));
    secure_clear(&key_check, sizeof(key_check));
    return ok;
}

static int verify_active_password(void) {
    char password[PASSWORD_MAX] = {0};
    uint64_t candidate = 0;
    VaultMeta meta;
    int ok = read_password("Current vault password: ", password, sizeof(password)) &&
             load_meta(&meta) && unlock_with_password(&meta, password, &candidate) &&
             ((candidate ^ active_vault_key) == 0);
    secure_clear(password, sizeof(password));
    secure_clear(&candidate, sizeof(candidate));
    secure_clear(&meta, sizeof(meta));
    if (!ok) puts("Password verification failed.");
    return ok;
}

static int save_rewrapped_meta(const char *new_password) {
    VaultMeta meta;
    if (!load_meta(&meta)) {
        fputs("Vault metadata is missing or damaged.\n", stderr);
        return 0;
    }
    if (!vault_random_bytes(&meta.salt, sizeof(meta.salt))) {
        secure_clear(&meta, sizeof(meta));
        return 0;
    }
    uint64_t derived = derive_password_value(new_password, meta.salt);
    meta.password_check = mix64(derived ^ password_domain);
    meta.wrapped_key = active_vault_key ^ mix64(derived ^ wrap_domain);
    meta.key_check = mix64(active_vault_key ^ key_domain);
    int ok = write_meta_file(VAULT_META_TEMP, &meta);
    secure_clear(&derived, sizeof(derived));
    secure_clear(&meta, sizeof(meta));
    return ok;
}

int vault_auth_recover(void) {
    int have_meta = path_exists(VAULT_META_FILE);
    int have_secret = path_exists(VAULT_FILE);
    int backup_meta = path_exists(VAULT_META_BACKUP);
    int backup_secret = path_exists(VAULT_SECRET_BACKUP);
    if (have_meta && have_secret) {
        if (backup_meta) remove(VAULT_META_BACKUP);
        if (backup_secret) remove(VAULT_SECRET_BACKUP);
        remove(VAULT_META_TEMP);
        remove(VAULT_SECRET_TEMP);
        return 1;
    }
    if (backup_meta && backup_secret) {
        if (have_meta) remove(VAULT_META_FILE);
        if (have_secret) remove(VAULT_FILE);
        if (rename(VAULT_META_BACKUP, VAULT_META_FILE) != 0 ||
            rename(VAULT_SECRET_BACKUP, VAULT_FILE) != 0) {
            fputs("Could not recover the previous vault files.\n", stderr);
            return 0;
        }
        remove(VAULT_META_TEMP);
        remove(VAULT_SECRET_TEMP);
        return 1;
    }
    if (backup_meta) {
        if (have_meta) remove(VAULT_META_BACKUP);
        else if (rename(VAULT_META_BACKUP, VAULT_META_FILE) != 0) return 0;
    }
    if (backup_secret) {
        if (have_secret) remove(VAULT_SECRET_BACKUP);
        else if (rename(VAULT_SECRET_BACKUP, VAULT_FILE) != 0) return 0;
    }
    remove(VAULT_META_TEMP);
    remove(VAULT_SECRET_TEMP);
    return 1;
}

static int replace_pair(const VaultMeta *meta, uint64_t new_key) {
    if (!write_meta_file(VAULT_META_TEMP, meta) ||
        !write_empty_secret_file(VAULT_SECRET_TEMP)) {
        remove(VAULT_META_TEMP);
        remove(VAULT_SECRET_TEMP);
        return 0;
    }
    remove(VAULT_META_BACKUP);
    remove(VAULT_SECRET_BACKUP);
    int backed_meta = 0, backed_secret = 0;
    if (path_exists(VAULT_META_FILE)) {
        if (rename(VAULT_META_FILE, VAULT_META_BACKUP) != 0) {
            perror("Backing up vault metadata");
            goto failed;
        }
        backed_meta = 1;
    }
    if (path_exists(VAULT_FILE)) {
        if (rename(VAULT_FILE, VAULT_SECRET_BACKUP) != 0) {
            perror("Backing up vault messages");
            goto rollback;
        }
        backed_secret = 1;
    }
    if (rename(VAULT_SECRET_TEMP, VAULT_FILE) != 0 ||
        rename(VAULT_META_TEMP, VAULT_META_FILE) != 0) {
        perror("Installing reset vault");
        remove(VAULT_FILE);
        remove(VAULT_META_FILE);
        goto rollback;
    }
    if (backed_meta) remove(VAULT_META_BACKUP);
    if (backed_secret) remove(VAULT_SECRET_BACKUP);
    active_vault_key = new_key;
    active = 1;
    return 1;

rollback: {
        int restored = 1;
        if (backed_meta && rename(VAULT_META_BACKUP, VAULT_META_FILE) != 0) restored = 0;
        if (backed_secret && rename(VAULT_SECRET_BACKUP, VAULT_FILE) != 0) restored = 0;
        if (!restored) fputs("Automatic rollback was incomplete; backup files were retained.\n", stderr);
    }
failed:
    remove(VAULT_META_TEMP);
    remove(VAULT_SECRET_TEMP);
    return 0;
}

int vault_auth_key(uint64_t *key) {
    if (!active || !key) return 0;
    *key = active_vault_key;
    return 1;
}

int vault_is_unlocked(void) { return active; }
int vault_is_configured(void) { return path_exists(VAULT_META_FILE) && path_exists(VAULT_FILE); }

void vault_lock(void) {
    secure_clear(&active_vault_key, sizeof(active_vault_key));
    active = 0;
}

int vault_create(void) {
    if (path_exists(VAULT_META_FILE) || path_exists(VAULT_FILE)) {
        puts("Vault files already exist. Use Unlock, or reset the vault.");
        return 0;
    }
    char password[PASSWORD_MAX] = {0};
    uint64_t key = 0;
    VaultMeta meta;
    memset(&meta, 0, sizeof(meta));
    int ok = prompt_new_password(password) && initialize_meta(&meta, password, &key);
    secure_clear(password, sizeof(password));
    if (ok) ok = write_empty_secret_file(VAULT_SECRET_TEMP) &&
                 write_meta_file(VAULT_META_TEMP, &meta);
    if (ok && rename(VAULT_SECRET_TEMP, VAULT_FILE) != 0) {
        perror(VAULT_FILE); ok = 0;
    }
    if (ok && rename(VAULT_META_TEMP, VAULT_META_FILE) != 0) {
        perror(VAULT_META_FILE); remove(VAULT_FILE); ok = 0;
    }
    if (ok) {
        active_vault_key = key;
        active = 1;
        puts("Vault created and unlocked for this session.");
    } else {
        remove(VAULT_META_TEMP);
        remove(VAULT_SECRET_TEMP);
    }
    secure_clear(&key, sizeof(key));
    secure_clear(&meta, sizeof(meta));
    return ok;
}

int vault_unlock(void) {
    if (!vault_is_configured()) {
        puts("Create a vault before unlocking it.");
        return 0;
    }
    char password[PASSWORD_MAX] = {0};
    uint64_t key = 0;
    VaultMeta meta;
    memset(&meta, 0, sizeof(meta));
    int ok = read_password("Vault password: ", password, sizeof(password)) &&
             load_meta(&meta) && unlock_with_password(&meta, password, &key);
    secure_clear(password, sizeof(password));
    secure_clear(&meta, sizeof(meta));
    if (!ok) {
        secure_clear(&key, sizeof(key));
        vault_lock();
        puts("Could not unlock the vault. Check the password or vault files.");
        return 0;
    }
    active_vault_key = key;
    active = 1;
    secure_clear(&key, sizeof(key));
    puts("Vault unlocked for this session.");
    return 1;
}

int vault_change_password(void) {
    if (!active) { puts("Unlock the vault first."); return 0; }
    if (!verify_active_password()) return 0;
    char password[PASSWORD_MAX] = {0};
    int ok = prompt_new_password(password) && save_rewrapped_meta(password);
    secure_clear(password, sizeof(password));
    if (ok && rename(VAULT_META_TEMP, VAULT_META_FILE) != 0) {
        perror("Updating vault password"); ok = 0;
    }
    if (!ok) remove(VAULT_META_TEMP);
    else puts("Vault password changed. Existing messages remain available.");
    return ok;
}

int vault_reset(void) {
    if (!vault_auth_recover()) return 0;
    char confirmation[32];
    fputs("Reset discards all messages and replaces the vault. Type RESET to continue: ", stdout);
    fflush(stdout);
    if (!fgets(confirmation, sizeof(confirmation), stdin)) return 0;
    trim_newline(confirmation);
    if (strcmp(confirmation, "RESET") != 0) { puts("Reset cancelled."); return 0; }
    char password[PASSWORD_MAX] = {0};
    uint64_t key = 0;
    VaultMeta meta;
    memset(&meta, 0, sizeof(meta));
    int ok = prompt_new_password(password) && initialize_meta(&meta, password, &key);
    secure_clear(password, sizeof(password));
    if (ok) ok = replace_pair(&meta, key);
    if (ok) puts("Vault reset. The new vault is unlocked and empty.");
    else { remove(VAULT_META_TEMP); remove(VAULT_SECRET_TEMP); }
    secure_clear(&key, sizeof(key));
    secure_clear(&meta, sizeof(meta));
    return ok;
}
