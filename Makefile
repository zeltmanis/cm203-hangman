CC = gcc
CFLAGS_COMMON = -Wall -Wextra -g -std=c11

# Default build: deliberately vulnerable for the overflow demo
#   -O0                   no optimization, predictable stack layout
#   -fno-stack-protector  no canary, so the overflow isn't caught at runtime
#   -no-pie               fixed load address, easier to reason about
CFLAGS_DEMO = $(CFLAGS_COMMON) -O0 -fno-stack-protector -no-pie

# Sanitizer build: catches the same overflow + leaks + use-after-free
CFLAGS_ASAN = $(CFLAGS_COMMON) -fsanitize=address -fsanitize=undefined

SRCS = hangman.c

.PHONY: all asan clean

all: hangman

hangman: $(SRCS)
	$(CC) $(CFLAGS_DEMO) -o $@ $(SRCS)

asan: $(SRCS)
	$(CC) $(CFLAGS_ASAN) -o hangman_asan $(SRCS)

clean:
	rm -f hangman hangman_asan *.o
