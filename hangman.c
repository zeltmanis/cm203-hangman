#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    char *secret;
    char *mask;
    size_t len;
    int wrong;
    size_t remaining;
    char name[16];      /* unsafe scanf target */
    int max_wrong;      /* deliberately adjacent to name — overflow flips lives */
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
    memset(gs.name, 0, sizeof gs.name);
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

static void show_struct_layout(void) {
    printf("--- GameState layout ---\n");
    printf("  sizeof(GameState)        = %zu\n", sizeof(GameState));
    printf("  offsetof(secret)         = %zu\n", offsetof(GameState, secret));
    printf("  offsetof(mask)           = %zu\n", offsetof(GameState, mask));
    printf("  offsetof(len)            = %zu\n", offsetof(GameState, len));
    printf("  offsetof(wrong)          = %zu\n", offsetof(GameState, wrong));
    printf("  offsetof(remaining)      = %zu\n", offsetof(GameState, remaining));
    printf("  offsetof(name)           = %zu\n", offsetof(GameState, name));
    printf("  offsetof(max_wrong)      = %zu  <-- right after name[16]\n",
           offsetof(GameState, max_wrong));
    printf("------------------------\n\n");
}

static void show_counters(const GameState *gs, const char *when) {
    printf("--- counters %s ---\n", when);
    printf("  &gs.name      = %p   gs.name      = \"%.16s\"\n",
           (void *)gs->name, gs->name);
    printf("  &gs.max_wrong = %p   gs.max_wrong = %d\n",
           (void *)&gs->max_wrong, gs->max_wrong);
    printf("  &gs.wrong     = %p   gs.wrong     = %d\n",
           (void *)&gs->wrong, gs->wrong);
    printf("---------------------------------\n\n");
}

/* Deliberately UNSAFE: scanf("%s") has no length check.
 * Compiles with a warning, which is the point. */
static void read_name(GameState *gs) {
    printf("enter your name: ");
    scanf("%s", gs->name);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {} /* drain rest of line */
}

int main(void) {
    srand((unsigned)time(NULL));

    int n = 0;
    char **words = load_words("words.txt", &n);

    int idx = rand() % n;
    GameState gs = new_game(words[idx], 6);

    show_struct_layout();
    show_counters(&gs, "before name input");
    read_name(&gs);
    show_counters(&gs, "after name input");

    printf("welcome %.16s! the word has %zu letters.\n\n", gs.name, gs.len);

    while (gs.remaining > 0 && gs.wrong < gs.max_wrong) {
        print_mask(&gs);
        printf("(%d/%d wrong) guess a letter: ", gs.wrong, gs.max_wrong);

        char input[16];
        if (!fgets(input, sizeof input, stdin)) break;
        do_turn(&gs, input[0]);
    }

    if (gs.remaining == 0) {
        print_mask(&gs);
        printf("you win, %.16s!\n", gs.name);
    } else {
        printf("you lose, %.16s. the word was \"%s\"\n", gs.name, gs.secret);
    }

    /* Cleanup is skipped when LEAK=1 — demonstrates the leak under valgrind.
     * With LEAK=1 we leak gs.mask plus every word plus the array itself. */
    const int leak = getenv("LEAK") != NULL;
    const int uaf  = getenv("USE_AFTER_FREE") != NULL;

    if (!leak) {
        free_game(&gs);
        free_words(words, n);
    }

    /* Use-after-free demo: gs.secret points into words[idx], which we just
     * freed. Reading it now is a UAF.
     *   ./hangman_asan with USE_AFTER_FREE=1     → ASan red wall
     *   valgrind ./hangman with USE_AFTER_FREE=1 → "Invalid read of size N"
     * Default build may print correct-looking output by luck — that's the
     * danger of UAFs: tests can pass while the bug is still present. */
    if (uaf && !leak) {
        printf("\n[UAF demo] reading freed memory at gs.secret = \"%s\"\n",
               gs.secret);
    }
    return 0;
}
