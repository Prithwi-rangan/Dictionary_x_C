# WordMate

WordMate is a modular C dictionary and vocabulary learning program. It keeps entries in 26 alphabetically sorted text files under `data/dictionary/` and uses small binary indexes for quick searches. On first launch, it imports `azdictionary.txt` if that file is beside the program.

## Build and run

```sh
make
./wordmate
./wordmate search apple
./wordmate exam 10
```

`make` creates `data/dictionary/` when needed. The first launch builds the A-Z dictionary and indexes. Keep `azdictionary.txt` beside the program during that import.

## Commands

```text
./wordmate search apple
./wordmate add sesquipedalian "characterized by long words"
./wordmate update sesquipedalian "given to using long words"
./wordmate delete sesquipedalian
./wordmate random
./wordmate memorize apple
./wordmate memorized
./wordmate stats
./wordmate exam 30
./wordmate hangman
./wordmate history
```

Search checks spelling automatically when a word is not found. After a correctly spelled word is found, WordMate asks whether to add it to Memorized Words. The interactive menu also has add, update, delete, Word of the Day, memorized words, dictionary statistics, vocabulary games, and a combined activity history and performance view. After each menu action, press Enter to return to the menu or `0` to quit.

Vocabulary exams offer 10 or 30 multiple-choice questions. Project Hangman gives one word, a meaning clue, six wrong guesses, and a gallows drawing that builds as guesses are missed. Exam and Hangman results are appended to `data/activity_results.tsv`.

## Source files

- `wordmate.h`: shared types, constants and function declarations
- `main.c`, `menu.c`: command-line and interactive menu
- `dictionary.c`: import, sorted A-Z files, indexes, search, editing, spelling suggestions and game-word loading
- `learning.c`: memorized words with meanings
- `games.c`: 10 or 30 question multiple-choice exams and Project Hangman
- `results.c`: activity history and performance summary
- `util.c`: input and word helpers
- `Makefile`: build and clean targets
