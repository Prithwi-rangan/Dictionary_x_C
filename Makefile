CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=
TARGET = wordmate
SOURCES = main.c menu.c dictionary.c learning.c games.c results.c vault.c vault_auth.c util.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c wordmate.h
	$(CC) $(CFLAGS) -c $< -o $@

vault.o vault_auth.o: vault_internal.h

clean:
	rm -f $(TARGET) $(OBJECTS)
