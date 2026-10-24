#ifndef WORDMATE_VAULT_INTERNAL_H
#define WORDMATE_VAULT_INTERNAL_H

#include "wordmate.h"

/* The binary vault keeps a bounded number of fixed-size encrypted messages. */
#define VAULT_MAX_MESSAGES 20

/* Exact record written to secret.txt; magic identifies this file format. */
typedef struct {
    char magic[8];
    unsigned long password_hash;
    unsigned int message_count;
    char messages[VAULT_MAX_MESSAGES][MEANING_MAX];
} VaultData;

/* Shared file I/O and in-memory lock state used by both vault source files. */
int vault_load_data(VaultData *data);
int vault_save_data(const VaultData *data);
int vault_require_unlocked(void);
void vault_set_unlocked(int unlocked);

#endif
