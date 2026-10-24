# Compiler settings can be overridden on the make command line.
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
TARGET = wordmate
SOURCES = main.c menu.c dictionary.c learning.c games.c results.c vault.c vault_auth.c util.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean prepare
all: $(TARGET)

# Create the writable data folders before any source file is compiled or run.
prepare:
	mkdir -p data/dictionary

# This order-only prerequisite makes setup finish before parallel compilation starts.
$(OBJECTS): | prepare

# Link all module objects into the single command-line program.
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c wordmate.h
	$(CC) $(CFLAGS) -c $< -o $@

vault.o vault_auth.o: vault_internal.h

# Remove generated build outputs without touching dictionary or user data files.
clean:
	rm -f $(TARGET) $(OBJECTS)
