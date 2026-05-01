#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char *secret;
    char *mask;
    size_t len;
    int wrong;
    int max_wrong;
    size_t remaining;
} GameState;

static char **load_words(const char *path, int *count) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        exit(1);
    }

    int capacity = 16;
    int n = 0;
    char **words = malloc(capacity * sizeof(char *));

    char line[64];
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        if (n == capacity) {
            capacity *= 2;
            words = realloc(words, capacity * sizeof(char *));
        }

        words[n] = malloc(len + 1);
        memcpy(words[n], line, len + 1);
        n++;
    }

    fclose(f);
    *count = n;
    return words;
}

static void free_words(char **words, int count) {
    for (int i = 0; i < count; i++) {
        free(words[i]);
    }
    free(words);
}

static GameState new_game(char *secret, int max_wrong) {
    GameState gs;
    gs.secret = secret;
    gs.len = strlen(secret);
    gs.mask = malloc(gs.len + 1);
    for (size_t i = 0; i < gs.len; i++) gs.mask[i] = '_';
    gs.mask[gs.len] = '\0';
    gs.wrong = 0;
    gs.max_wrong = max_wrong;
    gs.remaining = gs.len;
    return gs;
}

static void free_game(GameState *gs) {
    free(gs->mask);
}

static void print_mask(const GameState *gs) {
    printf("word: ");
    for (size_t i = 0; i < gs->len; i++) printf("%c ", gs->mask[i]);
    printf("\n");
}

static int do_turn(GameState *gs, char guess) {
    int hits = 0;
    for (size_t i = 0; i < gs->len; i++) {
        if (gs->secret[i] == guess && gs->mask[i] == '_') {
            gs->mask[i] = guess;
            hits++;
        }
    }
    if (hits > 0) gs->remaining -= hits;
    else gs->wrong++;
    return hits;
}

int main(void) {
    srand((unsigned)time(NULL));

    int n = 0;
    char **words = load_words("words.txt", &n);

    int idx = rand() % n;
    GameState gs = new_game(words[idx], 6);

    while (gs.remaining > 0 && gs.wrong < gs.max_wrong) {
        print_mask(&gs);
        printf("(%d/%d wrong) guess a letter: ", gs.wrong, gs.max_wrong);

        char input[16];
        if (!fgets(input, sizeof input, stdin)) break;
        do_turn(&gs, input[0]);
    }

    if (gs.remaining == 0) {
        print_mask(&gs);
        printf("you win!\n");
    } else {
        printf("you lose. the word was \"%s\"\n", gs.secret);
    }

    free_game(&gs);
    free_words(words, n);
    return 0;
}
