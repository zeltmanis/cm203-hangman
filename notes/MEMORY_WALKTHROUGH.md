# Memory walkthrough — for the cm203-hangman team

This document explains, step by step, what the hangman game does in
memory and how the inspector (left pane) visualizes it. It is written
for someone who has never used C before. Read it top to bottom, or
jump to whichever section you need.

**Contents**

1. What memory actually is
2. Stack vs heap
3. How to read the inspector view
4. The C code, line by line (the parts that matter)
5. The five demos, explained simply
6. What to read in *The C Book*

---

## 1. What memory actually is

Imagine a very long row of numbered boxes, each box holding one byte
(a number from 0 to 255). The boxes are numbered starting from 0 going
up to a huge number. Each box's number is its **address**.

```
  address:  0    1    2    3    4    5    6    7    8 ...
  contents: [..][..][..][..][..][..][..][..][..]...
```

When you see something like `0x7ffd1e5aecd8` in the inspector, that's
just one of those box numbers, written in **hexadecimal** (base 16).
It's a big number because modern computers have addresses up to
2⁶⁴ — about 18 billion billion boxes.

Hexadecimal uses 0–9 and a–f. So `0x10` is decimal 16, `0xff` is
decimal 255, `0x100` is decimal 256. You don't need to do hex math
constantly — you mostly compare addresses to see which is higher or
lower, or how far apart they are.

Two addresses 16 apart means there are 16 bytes between them. So
when you see:

```
  &gs.name      = 0x7ffdb8d404a8
  &gs.max_wrong = 0x7ffdb8d404b8
```

`0x4b8 - 0x4a8 = 0x10 = 16`. The two variables are exactly 16 bytes
apart. That's why a 17-character name overflows from `name` into
`max_wrong`.

---

## 2. Stack vs heap

Memory is one big row of boxes, but the operating system carves it
into **regions**, each used for different things. The two we care
about are the **stack** and the **heap**.

```
  HIGH ADDRESSES  ─── 0x7ffd...
  ╔═════════════════════════════╗
  ║  STACK                       ║   ← grows DOWN as functions are called
  ║  (function variables, args) ║
  ║  ↓                           ║
  ║                              ║
  ║                              ║
  ║  ↑                           ║
  ║  HEAP                        ║   ← grows UP as malloc is called
  ║  (malloc'd memory)           ║
  ╚═════════════════════════════╝
  LOW ADDRESSES   ─── 0x000003e5...
```

### The stack

When the program calls a function, it pushes a "frame" onto the stack.
A frame holds that function's local variables. When the function
returns, the frame is automatically removed.

The stack is **automatic** — you don't manage it. Every time you
write `int x = 5;` inside a function, the compiler reserves space
for `x` on the stack. When the function ends, `x` is gone.

In the inspector you see one stack frame: `GameState gs` inside
`main()`. The address `0x7ffdb8d40480` is high — `7ff...` — exactly
where the stack lives.

### The heap

The heap is **manual**. You ask the C library for memory with
`malloc(n)` (give me n bytes). It returns an address. You can
keep that address as long as you want, until you call `free(addr)`
to return it.

If you forget to `free`, the memory stays reserved forever — that's
a **leak**. If you `free` and then keep using the address, that's a
**use-after-free**. Both are real bugs, both are demoed in this
project.

In the inspector, the heap addresses look like `0x389af910` — much
smaller numbers, low in memory. That's where `malloc` lives.

### Why two regions?

The stack is fast and free, but limited to a few megabytes and
forgets your data when the function ends. The heap is unlimited
(within reason) and persists, but you must clean up after yourself.

C exposes both directly. Higher-level languages like Python or Java
hide the heap behind garbage collection — you never call `free`,
the runtime does it for you. C makes you do it by hand. That's why
malloc/free errors are some of the most common bugs in real
software.

---

## 3. How to read the inspector view

The inspector has three sections:

### The HEAP section

```
  HEAP   (12 live allocations)
  ============================
  0x389af670  [ 128 bytes]  words[]
  0x389af910  [   9 bytes]  word[0]   = "elephant"
  0x389af930  [   7 bytes]  word[1]   = "banana"
  ...
  0x389afa50  [   7 bytes]  mask      = "______"
```

Each line is one block of memory we got from `malloc`. The pieces:

- **`0x389af670`** — the address `malloc` returned (where the block
  starts).
- **`[ 128 bytes]`** — how big the block is.
- **`words[]`** — a label *we* attached so we know what it's for
  (these labels come from our `inspector_alloc(...)` calls in the
  C code).
- **`= "elephant"`** — the contents (when the block holds a string).

Notice the addresses go up: `0x389af670`, `0x389af910`, `0x389af930`,
... That's the heap growing as `malloc` is called repeatedly.

### The STACK section

```
  STACK
  ============================
  0x7ffdb8d40480  [  64 bytes]  GameState
  ----------------------------
    +0   secret    ( 8b)  = 0x389af930  -> heap [word[1]]
    +8   mask      ( 8b)  = 0x389afa50  -> heap [mask]
    +16  len       ( 8b)  = 6
    +24  wrong     ( 4b)  = 0
    +32  remaining ( 8b)  = 6
    +40  name      (16b)  = 'timasdasdasdhjka'
    +56  max_wrong ( 4b)  = 1718117491
```

This is the `GameState` struct, which is one stack-allocated object.
Each indented line is a **field** inside it:

- **`+0`, `+8`, `+16`, ...** — the field's *offset* inside the struct
  (in bytes). The struct begins at `0x...0480`. The `secret` field
  starts at offset 0, the `mask` field at offset 8, and so on.
- **`secret`** — the field's name in the C code.
- **`( 8b)`** — the field's size in bytes. Pointers are 8 bytes
  on 64-bit systems. Integers are 4. `name` is 16 bytes (a 16-element
  char array).
- **`= 0x389af930`** — the field's current value.
- **`-> heap [word[1]]`** — when a value is itself an address into the
  heap, the inspector follows it and shows what it points at. Here,
  `secret` holds the address of `word[1]`, which is "banana". So the
  secret of this game is "banana".

### The RECENT EVENTS section

The raw event log, last 6 lines. Useful for understanding what
caused the most recent change. For example, an `ALLOC` event means
`malloc` just happened.

---

## 4. The C code, line by line

Here we walk through the parts of `hangman.c` that drive what you
see in the inspector.

### 4.1 The `GameState` struct (lines 7–15)

```c
typedef struct {
    char *secret;     // pointer to the secret word (lives on the heap)
    char *mask;       // pointer to the mask string (heap)
    size_t len;       // length of the secret word
    int wrong;        // how many wrong guesses so far
    size_t remaining; // how many letters still hidden
    char name[16];    // player's name (16-byte buffer on the stack)
    int max_wrong;    // how many wrong guesses are allowed before losing
} GameState;
```

A `struct` is a custom data type — like a labeled box that holds
several smaller boxes inside. We declared one called `GameState` that
holds 7 fields.

When we write `GameState gs;` in `main()`, the compiler reserves
**all 64 bytes at once** as a single chunk on the stack. The fields
live at fixed offsets within that chunk. Those offsets are what the
inspector shows as `+0`, `+8`, `+16`, etc.

The order matters. `name` is placed right before `max_wrong` on
purpose — that's what makes the buffer overflow demo work.

### 4.2 Loading words from disk (`load_words`, lines 78–113)

```c
static char **load_words(const char *path, int *count) {
    FILE *f = fopen(path, "r");           // open words.txt
    ...
    char **words = malloc(capacity * sizeof(char *));
    inspector_alloc(words, ..., "words[]");

    char line[64];                         // 64-byte stack buffer
    while (fgets(line, sizeof line, f)) {  // read one line
        ...
        words[n] = malloc(len + 1);        // one malloc per word
        memcpy(words[n], line, len + 1);   // copy the word in
        inspector_alloc(words[n], ..., "word[N]");
        inspector_write(words[n], ...);    // record what we wrote
        n++;
    }
    ...
    return words;
}
```

Walking through what happens:

1. We open `words.txt` for reading.
2. We `malloc` an array of pointers (the `words` array). That's the
   first heap allocation you see in the inspector — `words[]` at
   128 bytes (16 slots × 8 bytes per pointer).
3. For each line in the file, we `malloc` a new buffer just big
   enough for the word plus its null terminator (`+1`), and copy
   the word into it.
4. Each `malloc` triggers an `inspector_alloc` call, which writes
   an `ALLOC` line to the log file. The Python inspector reads
   those lines and adds them to the HEAP section.

After this function runs, the inspector shows 11 allocations:
the `words[]` array plus 10 word strings.

### 4.3 The unsafe name input (`read_name`, lines 195–202)

```c
static void read_name(GameState *gs) {
    printf("enter your name: ");
    scanf("%s", gs->name);   /* DELIBERATELY UNSAFE */
    ...
}
```

`scanf("%s", ...)` reads characters into a buffer until whitespace,
**without checking the buffer's size**. If the user types more than
15 characters (16 minus 1 for the null terminator), the extra bytes
spill past `name` into whatever comes next in memory.

What comes next in memory is `max_wrong` (offset 56, right after
`name` which ends at offset 40+16=56). That's why typing a long name
corrupts `max_wrong`.

### 4.4 The inspector hooks

We added these helper functions at the top of `hangman.c`:

```c
static void inspector_alloc(void *addr, size_t size, const char *label);
static void inspector_free(void *addr);
static void inspector_write(void *addr, const char *value);
static void inspector_stack(void *addr, size_t size, const char *label);
static void inspector_field(void *parent, size_t offset, ...);
static void inspector_note(const char *msg);
```

When `INSPECTOR_LOG` is set in the environment, these functions
write event lines to the log file. When it isn't set, they do
nothing (zero overhead — the program runs identically without
the inspector).

This is the **separation of concerns**: the game code calls into
the inspector hooks at meaningful moments (after every `malloc`,
after every state change), and the Python inspector reads the log
and renders the picture.

---

## 5. The five demos, explained simply

### Demo 1 — Buffer overflow (infinite lives)

Type a normal name like `John`: nothing exciting, `max_wrong` stays 6.

Type a 20+ character name like `AAAAAAAAAAAAAAAAAAAA`: the inspector
shows `name = 'AAAAAAAAAAAAAAAA'` and `max_wrong = 1094795585`.

What happened: each 'A' is the byte `0x41`. `name` is 16 bytes long.
Bytes 17–20 of your input land in `max_wrong`. Four 'A's give
`0x41414141` = 1,094,795,585 in decimal. The wrong-guess limit just
went from 6 to over a billion. **You can't lose.**

In the inspector, you can literally see the corrupted value side by
side with the un-corrupted `wrong`. Both are integers, both lived next
to each other on the stack, but only `max_wrong` got hit because the
buffer overflowed in *one* direction.

This is the classic stack buffer overflow — the same family of bug
behind decades of security exploits.

### Demo 2 — Stack-local overflow ASan catches

Same idea but the buffer lives in a small standalone program
(`demos/name_overflow.c`). When built with AddressSanitizer (`make
asan_demo`) and given a long name, ASan prints a bright red error
message pointing at the exact line where the bad write happened.

Why does ASan catch this one but not Demo 1? In Demo 1 the buffer is
inside a struct, and ASan tracks the whole struct as one block.
A write that stays inside the struct (overflowing one field into
the next) doesn't cross the block boundary. In Demo 2 the buffer is
a stand-alone stack array, so ASan puts "redzones" around it and
catches the overflow immediately.

**Real-world lesson:** sanitizers are powerful but have blind spots.

### Demo 3 — Heap leak (LEAK=1)

Run `LEAK=1 ./hangman` — the program skips its cleanup at the end.
In the inspector, the heap stays full forever; no `FREE` events fire.

Run `LEAK=1 valgrind ./hangman` — valgrind reports:
- "definitely lost: 137 bytes in 2 blocks"
- "indirectly lost: 82 bytes in 10 blocks"

The numbers match what we allocated. Valgrind even tells you the
*line number* in `hangman.c` where each leaked block was allocated.

**Real-world lesson:** every `malloc` needs exactly one matching
`free`. Forgetting is easy. Tools like valgrind catch it.

### Demo 4 — Use-after-free (USE_AFTER_FREE=1)

After cleanup, the program reads `gs.secret`. But `gs.secret` was
just a pointer into `words[idx]`, which we already freed. Reading
it now is **using freed memory**.

In the default build it might print correct-looking text (the
freed bytes haven't been reused yet) — that's the *danger*: bugs
like this can pass tests and survive into production.

In the ASan build, you get a red error with three stack traces:
where the access happened, where the memory was freed, and where
it was originally allocated.

### Demo 5 — Process memory cheat (`./demos/cheat_dump.sh`)

While the game is running in another terminal, this script reads
its memory map (`/proc/PID/maps`) and dumps the heap with `gcore`.
Then `strings` finds all 10 candidate words inside the dump.

**Real-world lesson:** any process you can ptrace, you can read
the memory of. C gives no built-in privacy boundary. Higher-level
sandboxes are built *on top* of these primitives.

---

## 6. What to read in *The C Book* (Banahan/Brady/Doran)

These map the project's concepts to specific chapters of the book
already in your `Computer Organization` folder. Read in this order
for the best build-up.

### Tier 1 — must read before the presentation

| Topic | Chapter | Why we use it |
|---|---|---|
| Variables, types, sizes | 2 | Explains why `int` is 4 bytes and `char *` is 8 bytes — drives all the offsets. |
| Arrays and pointers | 5 | Spine of the project. `char **words`, pointer arithmetic, decay. |
| Structures | 6 | The `GameState` struct, padding, `offsetof`. |
| Functions | 4 | Stack frames are created by function calls. |

### Tier 2 — must understand for Q&A

| Topic | Where in *The C Book* | Why we use it |
|---|---|---|
| `malloc` / `free` / `realloc` | Ch. 9, `<stdlib.h>` section | The heap demos depend on understanding these. |
| String functions | Ch. 9, `<string.h>` section | `strcpy`, `strlen`, `memcpy` — and why `gets`/`scanf("%s")` are dangerous. |
| Function pointers | Ch. 5, function-pointer section | Optional but powerful — useful if asked. |
| The preprocessor | Ch. 7 | `#include`, `#define`. Mostly mechanical. |

### Tier 3 — skim for breadth

| Topic | Where | Why |
|---|---|---|
| Control flow | Ch. 3 | Already familiar from other languages. Skim. |
| `const`, `volatile` | Ch. 8 (start of) | Might come up in Q&A; one paragraph each is enough. |
| File I/O | Ch. 9, `<stdio.h>` | We use `fopen`/`fgets` to load words. |

### Beyond the book — modern tooling

The book is from 1991 and predates the tools we depend on. Spend
~30 minutes total on:

- **AddressSanitizer**: read the project README on GitHub
  (google "AddressSanitizer wiki"). What `-fsanitize=address` does,
  what shadow bytes are.
- **Valgrind memcheck**: read `valgrind --help` and the man page's
  "definitely lost / indirectly lost / possibly lost / still
  reachable" definitions.
- **`/proc/PID/maps`**: one paragraph in `man proc` explains the
  format. Each line is a memory region with permissions and what
  file (if any) it's mapped from.
- **Compiler flags for the demo**: `-fno-stack-protector`,
  `-no-pie`, `-O0`. Be ready to explain *why* you turned them off.

### Things you can safely skip

- Most of Chapter 8 beyond `const` (rare at this level).
- Chapter 10 (complete programs) — your project replaces this.
- Historical asides about K&R vs. ANSI C — interesting, but not
  going to help your score.

---

## How to use this document for the presentation

Each team member should be able to explain at least:

- What memory looks like (Section 1)
- The stack vs heap difference (Section 2)
- One full code path top to bottom (Section 4)
- Two of the five demos in detail (Section 5)

In the presentation, the inspector becomes a teaching aid. Point at
the screen and say things like:

> "Now I'm going to type a long name. Watch the `name` field —
> it fills with A's. **And watch `max_wrong` next to it** — it
> just changed from 6 to over a billion. The overflow wrote past
> the buffer into the field next door."

That's the demo. The inspector makes the *invisible* visible, which
is exactly what the project brief asked for.
