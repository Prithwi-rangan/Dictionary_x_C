#define _POSIX_C_SOURCE 200809L
#include "vault_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const uint64_t tag_domain = UINT64_C(0x574d2d5245434f52);

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

static void encode_header(const VaultRecordHeader *header, unsigned char bytes[16]) {
    put_u32(bytes, header->version);
    put_u32(bytes + 4, header->message_length);
    put_u64(bytes + 8, header->nonce);
}

static uint64_t mix64(uint64_t value) {
    value ^= value >> 30;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31;
    return value;
}

static unsigned char stream_byte(uint64_t key, uint64_t nonce, size_t position) {
    uint64_t block_number = (uint64_t)(position / 8u);
    uint64_t block = mix64(key ^ nonce ^ (block_number * UINT64_C(0x9e3779b97f4a7c15)));
    return (unsigned char)(block >> (8u * (unsigned)(position % 8u)));
}

/* This checksum is only a basic corruption check, not a secure MAC. */
static uint64_t record_tag(uint64_t key, const VaultRecordHeader *header,
                           const unsigned char *ciphertext) {
    unsigned char header_bytes[16];
    encode_header(header, header_bytes);
    uint64_t value = UINT64_C(14695981039346656037) ^ key ^ tag_domain;
    for (size_t i = 0; i < sizeof(header_bytes); ++i) {
        value ^= header_bytes[i];
        value *= UINT64_C(1099511628211);
    }
    for (uint32_t i = 0; i < header->message_length; ++i) {
        value ^= ciphertext[i];
        value *= UINT64_C(1099511628211);
    }
    secure_clear(header_bytes, sizeof(header_bytes));
    return mix64(value ^ key);
}

static int read_record(FILE *file, VaultRecordHeader *header,
                       unsigned char ciphertext[VAULT_MAX_MESSAGE], uint64_t *tag) {
    unsigned char header_bytes[16], tag_bytes[VAULT_TAG_SIZE];
    size_t got = fread(header_bytes, 1, sizeof(header_bytes), file);
    if (got == 0 && feof(file)) return 0;
    if (got != sizeof(header_bytes)) return -1;
    header->version = get_u32(header_bytes);
    header->message_length = get_u32(header_bytes + 4);
    header->nonce = get_u64(header_bytes + 8);
    if (header->version != VAULT_FORMAT_VERSION || header->message_length == 0 ||
        header->message_length > VAULT_MAX_MESSAGE) return -1;
    if (fread(ciphertext, 1, header->message_length, file) != header->message_length ||
        fread(tag_bytes, 1, sizeof(tag_bytes), file) != sizeof(tag_bytes)) return -1;
    *tag = get_u64(tag_bytes);
    secure_clear(header_bytes, sizeof(header_bytes));
    secure_clear(tag_bytes, sizeof(tag_bytes));
    return 1;
}

static int write_record(FILE *file, const VaultRecordHeader *header,
                        const unsigned char *ciphertext, uint64_t tag) {
    unsigned char header_bytes[16], tag_bytes[VAULT_TAG_SIZE];
    encode_header(header, header_bytes);
    put_u64(tag_bytes, tag);
    int ok = fwrite(header_bytes, 1, sizeof(header_bytes), file) == sizeof(header_bytes) &&
             fwrite(ciphertext, 1, header->message_length, file) == header->message_length &&
             fwrite(tag_bytes, 1, sizeof(tag_bytes), file) == sizeof(tag_bytes);
    secure_clear(header_bytes, sizeof(header_bytes));
    secure_clear(tag_bytes, sizeof(tag_bytes));
    return ok;
}

static int decode_record(uint64_t key, const VaultRecordHeader *header,
                         const unsigned char *ciphertext, uint64_t stored_tag,
                         char plaintext[MEANING_MAX]) {
    uint64_t expected_tag = record_tag(key, header, ciphertext);
    if ((expected_tag ^ stored_tag) != 0) {
        secure_clear(&expected_tag, sizeof(expected_tag));
        return 0;
    }
    for (uint32_t i = 0; i < header->message_length; ++i)
        plaintext[i] = (char)(ciphertext[i] ^ stream_byte(key, header->nonce, i));
    plaintext[header->message_length] = '\0';
    secure_clear(&expected_tag, sizeof(expected_tag));
    return 1;
}

int vault_add(const char *message) {
    uint64_t key = 0;
    if (!vault_auth_key(&key)) { puts("Unlock the vault first."); return 0; }
    size_t length = strlen(message);
    if (!length || length > VAULT_MAX_MESSAGE) {
        fprintf(stderr, "Message must contain 1-%d bytes.\n", VAULT_MAX_MESSAGE);
        secure_clear(&key, sizeof(key));
        return 0;
    }
    VaultRecordHeader header = {VAULT_FORMAT_VERSION, (uint32_t)length, 0};
    unsigned char ciphertext[VAULT_MAX_MESSAGE];
    int ok = vault_random_bytes(&header.nonce, sizeof(header.nonce));
    if (ok) {
        for (size_t i = 0; i < length; ++i)
            ciphertext[i] = (unsigned char)message[i] ^ stream_byte(key, header.nonce, i);
    }
    uint64_t tag = ok ? record_tag(key, &header, ciphertext) : 0;
    FILE *file = ok ? fopen(VAULT_FILE, "ab") : NULL;
    if (!file && ok) { perror(VAULT_FILE); ok = 0; }
    if (ok) ok = write_record(file, &header, ciphertext, tag) &&
                 fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (file && fclose(file) != 0) ok = 0;
    if (ok) (void)chmod(VAULT_FILE, S_IRUSR | S_IWUSR);
    secure_clear(ciphertext, sizeof(ciphertext));
    secure_clear(&key, sizeof(key));
    secure_clear(&tag, sizeof(tag));
    if (ok) puts("Message added to the vault.");
    else fputs("Could not store the message.\n", stderr);
    return ok;
}

int vault_list(void) {
    uint64_t key = 0;
    if (!vault_auth_key(&key)) { puts("Unlock the vault first."); return 0; }
    FILE *file = fopen(VAULT_FILE, "rb");
    if (!file) { perror(VAULT_FILE); secure_clear(&key, sizeof(key)); return 0; }
    VaultRecordHeader header;
    unsigned char ciphertext[VAULT_MAX_MESSAGE];
    char plaintext[MEANING_MAX];
    size_t index = 0;
    int status, shown = 0;
    while (1) {
        uint64_t tag = 0;
        status = read_record(file, &header, ciphertext, &tag);
        if (status <= 0) break;
        ++index;
        if (decode_record(key, &header, ciphertext, tag, plaintext)) {
            printf("%zu. %s\n", index, plaintext);
            shown = 1;
        } else {
            printf("%zu. [message data is damaged]\n", index);
        }
        secure_clear(plaintext, sizeof(plaintext));
        secure_clear(ciphertext, sizeof(ciphertext));
        secure_clear(&tag, sizeof(tag));
    }
    if (status < 0 || ferror(file)) fputs("Vault file is damaged or truncated.\n", stderr);
    fclose(file);
    if (!index && status == 0) puts("The vault is empty.");
    secure_clear(plaintext, sizeof(plaintext));
    secure_clear(ciphertext, sizeof(ciphertext));
    secure_clear(&key, sizeof(key));
    return status >= 0 && (shown || index == 0);
}

int vault_delete(size_t selected) {
    uint64_t key = 0;
    if (!vault_auth_key(&key)) { puts("Unlock the vault first."); return 0; }
    if (selected == 0) { puts("Message number must be greater than zero."); secure_clear(&key, sizeof(key)); return 0; }
    FILE *source = fopen(VAULT_FILE, "rb");
    if (!source) { perror(VAULT_FILE); secure_clear(&key, sizeof(key)); return 0; }
    FILE *temp = fopen(VAULT_SECRET_TEMP, "wb");
    if (!temp) { perror(VAULT_SECRET_TEMP); fclose(source); secure_clear(&key, sizeof(key)); return 0; }
    (void)chmod(VAULT_SECRET_TEMP, S_IRUSR | S_IWUSR);
    VaultRecordHeader header;
    unsigned char ciphertext[VAULT_MAX_MESSAGE];
    size_t index = 0;
    int removed = 0, ok = 1, status;
    while (ok) {
        uint64_t tag = 0;
        status = read_record(source, &header, ciphertext, &tag);
        if (status <= 0) break;
        ++index;
        if (index == selected) removed = 1;
        else if (!write_record(temp, &header, ciphertext, tag)) ok = 0;
        secure_clear(ciphertext, sizeof(ciphertext));
        secure_clear(&tag, sizeof(tag));
    }
    if (status < 0 || ferror(source)) ok = 0;
    if (fflush(temp) != 0 || fsync(fileno(temp)) != 0) ok = 0;
    if (fclose(source) != 0) ok = 0;
    if (fclose(temp) != 0) ok = 0;
    secure_clear(ciphertext, sizeof(ciphertext));
    secure_clear(&key, sizeof(key));
    if (ok && removed && rename(VAULT_SECRET_TEMP, VAULT_FILE) != 0) {
        perror("Replacing vault file"); ok = 0;
    }
    if (removed && ok) (void)chmod(VAULT_FILE, S_IRUSR | S_IWUSR);
    if (!ok || !removed) remove(VAULT_SECRET_TEMP);
    if (removed && ok) puts("Message deleted.");
    else if (!removed && ok) puts("No message has that number.");
    else fputs("Could not update the vault file.\n", stderr);
    return removed && ok;
}

static int read_message_line(char *buffer, size_t size) {
    if (!fgets(buffer, (int)size, stdin)) return 0;
    size_t length = strlen(buffer);
    if (length && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
        if (length && buffer[length - 1] == '\r') buffer[--length] = '\0';
        return 1;
    }
    if (!feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
        fputs("Input is too long.\n", stderr);
        buffer[0] = '\0';
        return 0;
    }
    return 1;
}

static int parse_message_number(const char *text, size_t *number) {
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    while (end && (*end == ' ' || *end == '\t')) ++end;
    if (errno || end == text || !end || *end != '\0' || value == 0 || value > SIZE_MAX) return 0;
    *number = (size_t)value;
    return 1;
}

void vault_menu(void) {
    if (!vault_auth_recover()) return;
    char input[LINE_MAX];
    for (;;) {
        printf("\n========== WORDMATE SECRET VAULT ==========\nStatus: %s\n"
               "1. Create vault\n2. Unlock vault\n3. Add secret message\n4. Read secret messages\n"
               "5. Delete a message\n6. Change password\n7. Reset entire vault\n0. Return to main menu\n",
               vault_is_unlocked() ? "UNLOCKED" : "LOCKED");
        fputs("Choose: ", stdout);
        if (!read_message_line(input, sizeof(input))) continue;
        char *end = NULL;
        errno = 0;
        long choice = strtol(input, &end, 10);
        while (end && (*end == ' ' || *end == '\t')) ++end;
        if (errno || end == input || !end || *end != '\0') {
            puts("Choose a number from the vault menu.");
            continue;
        }
        if (choice == 0) break;
        switch (choice) {
            case 1: vault_create(); break;
            case 2: vault_unlock(); break;
            case 3: {
                if (!vault_is_unlocked() && !vault_unlock()) break;
                char message[MEANING_MAX];
                fputs("Secret message: ", stdout); fflush(stdout);
                if (read_message_line(message, sizeof(message))) vault_add(message);
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
                fputs("Message number to delete: ", stdout); fflush(stdout);
                if (read_message_line(input, sizeof(input))) {
                    size_t selected = 0;
                    if (parse_message_number(input, &selected)) vault_delete(selected);
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
