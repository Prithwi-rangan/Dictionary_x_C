# WordMate

WordMate is a modular C terminal dictionary and vocabulary learning system. It imports the included `azdictionary.txt` on first launch and stores its working dictionary as one human-readable, alphabetically sorted text file per letter in `data/dictionary/`. A compact binary offset index for each letter enables binary search while preserving readable A-Z files.

## Build and run

```sh
make
./wordmate
./wordmate search apple
./wordmate exam 10 medium mixed
```

The first launch imports the supplied dictionary and can take a little time. Keep `azdictionary.txt` beside the executable on that first launch. After initialization, WordMate uses the files under `data/`.

## Commands

```text
./wordmate search apple
./wordmate add sesquipedalian "characterized by long words" "long-winded, polysyllabic"
./wordmate update sesquipedalian "given to using long words" "verbose"
./wordmate delete sesquipedalian
./wordmate random
./wordmate memorize apple
./wordmate memorized
./wordmate stats
./wordmate vault
./wordmate guess-word
./wordmate guess-meaning
./wordmate word-search
./wordmate history
./wordmate performance
```

Use quotes around multiword meanings or messages. Omitting a command opens the interactive menu. Select **Vocabulary games and tests** for the 10/20/50 question exam, difficulty and question type settings, guessing games and the 2D word-search game. Completed activities append to `data/activity_results.tsv`; use the history and performance options to review scores. The games load a compact sample of unique headwords and meanings from the same A-Z dictionary files.

## Secret Vault

The vault is implemented in C and builds with the same compiler as the rest of WordMate; it has no third-party library dependency. Open **Secret Vault** from the main menu or run `./wordmate vault`. Create a password on first use, then unlock to add, read, or delete messages. Password input is hidden in an interactive terminal. The password is not written to the vault files. Changing the password keeps existing messages; reset requires typing `RESET` and replaces the vault with an empty one.

**This is an educational C exercise, not real encryption.** The built-in password mixer, message transformation, and integrity check are custom algorithms and are not cryptographically secure. Do not store real passwords, financial information, or other sensitive data in this vault. The program uses the operating system's `/dev/urandom` for random values, but that does not make its homemade algorithms secure. Reset/deletion also does not promise forensic erasure from storage devices or backups.

## Files

- `wordmate.h`: shared types, constants, and prototypes
- `main.c`, `menu.c`: command-line handling and interactive menu
- `dictionary.c`: import, sorted A-Z files, persistent offset indexes, binary search, CRUD, spelling suggestions, Word of the Day, statistics, and the shared game-word pool
- `learning.c`: memorized-word storage
- `games.c`: MCQ exam, Guess the Word, Guess the Meaning, and 2D word search
- `results.c`: persistent activity history and performance summary
- `vault_auth.c`: C-only educational password handling, unlock, password change, and reset
- `vault.c`: C-only educational message transformation and the Secret Vault submenu
- `Makefile`: build and clean targets
