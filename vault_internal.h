#ifndef WORDMATE_VAULT_INTERNAL_H
#define WORDMATE_VAULT_INTERNAL_H

#include "wordmate.h"
#include <stdint.h>

#define VAULT_FORMAT_VERSION 1u
#define VAULT_MAX_MESSAGE (MEANING_MAX - 1)
#define VAULT_TAG_SIZE 8u
#define VAULT_META_TEMP "data/vault.meta.tmp"
#define VAULT_SECRET_TEMP "data/secrets.bin.tmp"
#define VAULT_META_BACKUP "data/vault.meta.bak"
#define VAULT_SECRET_BACKUP "data/secrets.bin.bak"

typedef struct {
    unsigned char magic[8];
    uint32_t version;
    uint64_t salt;
    uint64_t password_check;
    uint64_t wrapped_key;
    uint64_t key_check;
} VaultMeta;

typedef struct {
    uint32_t version;
    uint32_t message_length;
    uint64_t nonce;
} VaultRecordHeader;

int vault_auth_key(uint64_t *key);
int vault_auth_recover(void);
int vault_random_bytes(void *buffer, size_t length);

#endif
