#include <stdarg.h>
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

/* ------------------------------------------------------------------
 * Inspector instrumentation.
 *
 * When INSPECTOR_LOG=<path> is set in the environment, the program
 * writes one event per line to that path. A separate Python program
 * (inspector.py) reads the file and renders a live ASCII view of
 * stack and heap.
 *
 * If INSPECTOR_LOG is not set, all inspector_* calls are no-ops —
 * default behaviour is unchanged.
 * ------------------------------------------------------------------ */

static FILE *inspector_fp = NULL;

static void inspector_init(void) {
    const char *path = getenv("INSPECTOR_LOG");
    if (!path) return;
    inspector_fp = fopen(path, "w");
    if (!inspector_fp) return;
    setvbuf(inspector_fp, NULL, _IOLBF, 0);  /* line-buffered */
}

static void inspector_emit(const char *fmt, ...) {
    if (!inspector_fp) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(inspector_fp, fmt, ap);
    fputc('\n', inspector_fp);
    va_end(ap);
}

static void inspector_alloc(void *addr, size_t size, const char *label) {
    inspector_emit("ALLOC %p %zu %s", addr, size, label);
}

static void inspector_free(void *addr) {
    inspector_emit("FREE %p", addr);
}

static void inspector_write(void *addr, const char *value) {
    inspector_emit("WRITE %p \"%s\"", addr, value);
}

static void inspector_stack(void *addr, size_t size, const char *label) {
    inspector_emit("STACK %p %zu %s", addr, size, label);
}

static void inspector_field(void *parent, size_t offset, size_t size,
                            const char *name, const char *value) {
    inspector_emit("FIELD parent=%p offset=%zu size=%zu name=%s value=%s",
                   parent, offset, size, name, value);
}

static void inspector_note(const char *msg) {
    inspector_emit("NOTE %s", msg);
}

/* ------------------------------------------------------------------ */

static char **load_words(const char *path, int *count) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        exit(1);
    }

    int capacity = 16;
    int n = 0;
    char **words = malloc(capacity * sizeof(char *));
    inspector_alloc(words, capacity * sizeof(char *), "words[]");

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
            inspector_note("realloc words[] grew");
        }

        words[n] = malloc(len + 1);
        memcpy(words[n], line, len + 1);

        char label[32];
        snprintf(label, sizeof label, "word[%d]", n);
        inspector_alloc(words[n], len + 1, label);
        inspector_write(words[n], words[n]);

        n++;
    }

    fclose(f);
    *count = n;
    return words;
}

static void free_words(char **words, int count) {
    for (int i = 0; i < count; i++) {
        inspector_free(words[i]);
        free(words[i]);
    }
    inspector_free(words);
    free(words);
}

static GameState new_game(char *secret, int max_wrong) {
    GameState gs;
    gs.secret = secret;
    gs.len = strlen(secret);
    gs.mask = malloc(gs.len + 1);
    inspector_alloc(gs.mask, gs.len + 1, "mask");
    for (size_t i = 0; i < gs.len; i++) gs.mask[i] = '_';
    gs.mask[gs.len] = '\0';
    inspector_write(gs.mask, gs.mask);
    memset(gs.name, 0, sizeof gs.name);
    gs.wrong = 0;
    gs.max_wrong = max_wrong;
    gs.remaining = gs.len;
    return gs;
}

static void inspector_dump_state(const GameState *gs) {
    inspector_stack((void *)gs, sizeof *gs, "GameState");
    char buf[64];
    snprintf(buf, sizeof buf, "%p", (void *)gs->secret);
    inspector_field((void *)gs, offsetof(GameState, secret),
                    sizeof gs->secret, "secret", buf);
    snprintf(buf, sizeof buf, "%p", (void *)gs->mask);
    inspector_field((void *)gs, offsetof(GameState, mask),
                    sizeof gs->mask, "mask", buf);
    snprintf(buf, sizeof buf, "%zu", gs->len);
    inspector_field((void *)gs, offsetof(GameState, len),
                    sizeof gs->len, "len", buf);
    snprintf(buf, sizeof buf, "%d", gs->wrong);
    inspector_field((void *)gs, offsetof(GameState, wrong),
                    sizeof gs->wrong, "wrong", buf);
    snprintf(buf, sizeof buf, "%zu", gs->remaining);
    inspector_field((void *)gs, offsetof(GameState, remaining),
                    sizeof gs->remaining, "remaining", buf);
    snprintf(buf, sizeof buf, "'%.16s'", gs->name);
    inspector_field((void *)gs, offsetof(GameState, name),
                    sizeof gs->name, "name", buf);
    snprintf(buf, sizeof buf, "%d", gs->max_wrong);
    inspector_field((void *)gs, offsetof(GameState, max_wrong),
                    sizeof gs->max_wrong, "max_wrong", buf);
}

static void free_game(GameState *gs) {
    inspector_free(gs->mask);
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
    inspector_write(gs->mask, gs->mask);
    inspector_dump_state(gs);
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
    inspector_init();
    inspector_note("program start");

    srand((unsigned)time(NULL));

    int n = 0;
    char **words = load_words("words.txt", &n);

    int idx = rand() % n;
    GameState gs = new_game(words[idx], 6);
    inspector_dump_state(&gs);

    show_struct_layout();
    show_counters(&gs, "before name input");
    read_name(&gs);
    inspector_dump_state(&gs);
    inspector_note("name input complete (overflow may have happened)");
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
    } else {
        inspector_note("LEAK=1 — skipping cleanup, valgrind will see leaks");
    }

    /* Use-after-free demo: gs.secret points into words[idx], which we just
     * freed. Reading it now is a UAF. */
    if (uaf && !leak) {
        inspector_note("UAF — reading freed memory");
        printf("\n[UAF demo] reading freed memory at gs.secret = \"%s\"\n",
               gs.secret);
    }

    inspector_note("program end");
    if (inspector_fp) fclose(inspector_fp);
    return 0;
}
