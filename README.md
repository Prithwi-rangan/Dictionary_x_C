# WordMate

WordMate is a modular C dictionary and vocabulary learning program. It keeps dictionary entries in 26 alphabetically sorted text files under `data/dictionary/` and uses a small binary index for quick word searches. On first launch, it imports `azdictionary.txt` if that file is beside the program.

## Build and run

```sh
make
./wordmate
./wordmate search apple
./wordmate exam 10 medium mixed
```

`make` creates `data/dictionary/` when needed. The first launch builds the A-Z dictionary and indexes. Keep `azdictionary.txt` beside the program during that import.

## Commands

```text
./wordmate search apple
./wordmate synonyms apple
./wordmate add sesquipedalian "characterized by long words" "long-winded, polysyllabic"
./wordmate update sesquipedalian "given to using long words" "verbose"
./wordmate delete sesquipedalian
./wordmate random
./wordmate memorize apple
./wordmate memorized
./wordmate stats
./wordmate vault
./wordmate exam 30 medium mixed
./wordmate guess-word
./wordmate guess-meaning
./wordmate word-search
./wordmate history
./wordmate performance
```

The menu supports adding, updating, deleting and searching words; spelling suggestions; Word of the Day; synonyms; memorized words with meanings; dictionary statistics; and vocabulary games. Spelling suggestions keep the first letter fixed and show candidates with more than 70% matching letter positions after it. Examinations support 10, 30 or 50 multiple-choice questions. Guess the Word gives a meaning clue and five lives. The 2D word search uses a 10-by-10 grid. Exam and game results are appended to `data/activity_results.tsv`.

## Secret Vault

The vault stores its binary data in `data/secret.txt`. It asks for a password before opening the vault. Five incorrect password attempts clear that file. Secret messages are transformed with the substitution mapping `abcdefghijklmnopqrstuvwxyz` to `lpzqaokmjinxwsrvfdcehyubgt`.

This substitution is a simple programming exercise and does not provide real security. The password check is also a basic educational hash, and password input is visible while typing. Do not store sensitive information in the vault.

The older `data/secrets.bin` and `data/vault.meta` files from earlier versions are not used by this implementation and are left untouched.

## Source files

- `wordmate.h`: shared types, constants and function declarations
- `main.c`, `menu.c`: command-line and interactive menu
- `dictionary.c`: import, sorted A-Z files, indexes, search, editing, spelling suggestions and game-word loading
- `learning.c`: synonym lookup and memorized words with meanings
- `games.c`: MCQ exam, five-life Guess the Word, Guess the Meaning and 2D word search
- `results.c`: activity history and performance statistics
- `vault_auth.c`, `vault.c`: password gate and substitution-based secret file
- `util.c`: input and word helpers
- `Makefile`: build and clean targets
