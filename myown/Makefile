CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

wordmate: main.c menu.c dictionary.c util.c wordmate.h
	$(CC) $(CFLAGS) main.c menu.c dictionary.c util.c -o wordmate

clean:
	rm -f wordmate wordmate.exe
