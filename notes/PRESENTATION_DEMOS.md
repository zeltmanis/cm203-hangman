# Presentation demo script

The 8-minute live presentation, demo by demo. Each section says what
to **type**, what you'll **see**, what to **say**, and what the
**lesson** is.

Total time budget: **8 minutes** + 2 minutes Q&A.

---

## Layout

You need two terminal panes side by side (split terminal in VS Code,
or two tmux panes — whichever is most reliable on the presentation
laptop):

```
┌──────────────────────────┬──────────────────────────┐
│                          │                          │
│      LEFT PANE           │      RIGHT PANE          │
│      (inspector)         │      (game / tools)      │
│                          │                          │
│  ./inspector.py          │  ./hangman               │
│  /tmp/inspector.log      │                          │
│                          │                          │
│  Updates live as the     │  Where the typing        │
│  game runs.              │  happens.                │
│                          │                          │
└──────────────────────────┴──────────────────────────┘
```

For demos that don't need the inspector (the ASan / valgrind catches),
the left pane can show those tool outputs instead.

---

## Pre-presentation checklist (5 min before going on)

```bash
cd ~/cm203-hangman
git pull                              # latest version
make clean && make && make asan && make asan_demo
ls /tmp/inspector.log && rm /tmp/inspector.log    # clear stale log
```

Then split the terminal and start the inspector in the left pane:

```bash
./inspector.py /tmp/inspector.log
```

It will show empty heap and stack — that's correct, the game hasn't
started yet.

---

## Time budget (8 minutes)

| Time | Beat | Owner |
|---|---|---|
| 0:00–0:30 | Intro: "this is a microscope, not a game" | Anyone |
| 0:30–3:00 | Demo 1 — buffer overflow (Parts A + B) | Student A |
| 3:00–4:30 | Demo 2 — heap leak (valgrind) | Student B |
| 4:30–6:00 | Demo 3 — use-after-free (ASan) | Student B |
| 6:00–7:30 | Demo 4 — process memory cheat (gcore) | Student C |
| 7:30–8:00 | Wrap | Anyone |

Adjust ownership however your team prefers — what matters is each
of the three students owns at least one demo and presents one
distinct topic to the audience.

---

## Intro (30 seconds)

**Say:**

> "This is hangman. *(plays one normal letter on the right pane)*
> But hangman is the wrong thing to focus on. The actual project is
> about what's happening on the **left** — that's a custom inspector
> that shows the game's memory in real time. Stack frames at the
> bottom, heap allocations at the top. Everything you'll see for
> the next eight minutes is real, live, captured from the running
> program."

Then start Demo 1.

---

## Demo 1 — Buffer overflow → infinite lives (2:30)

This is the showcase demo. Two parts: A shows the bug working in
the main game; B shows AddressSanitizer catching the same class of
bug in a stand-alone version.

### Part A — overflow the game (90s)

**Setup**: in the right pane, restart the game so the inspector
starts fresh.

```bash
rm -f /tmp/inspector.log
INSPECTOR_LOG=/tmp/inspector.log ./hangman
```

The inspector pane should now show 12 heap allocations (the words
array + 10 words + the mask) and the `GameState` struct on the stack
with all fields visible.

**Say**:

> "Look at the inspector. The heap holds our 10 candidate words
> plus a mask. The stack holds one struct, `GameState`, with seven
> fields. Pay attention to two of them: `name` at offset 40, sixteen
> bytes wide, and `max_wrong` at offset 56, four bytes wide. They
> are immediately adjacent in memory. Watch what happens."

**Type**: when prompted "enter your name:", type 20 A's:

```
AAAAAAAAAAAAAAAAAAAA
```

**What you'll see in the inspector** (left pane updates):

```
+40  name      (16b)  = 'AAAAAAAAAAAAAAAA'
+56  max_wrong ( 4b)  = 1094795585
```

**Say**:

> "The `name` field now holds 16 A's. And `max_wrong`, which was
> 6, is now 1,094,795,585. That's `0x41414141` in hex — four ASCII
> 'A' bytes. The `scanf` call wrote 20 characters into a 16-byte
> buffer, and the extra four overflowed into `max_wrong` next door.
> The wrong-guess limit just went from 6 to over a billion. **I
> can't lose.**"

Show this by guessing a few wrong letters. The right pane will
print `(1/1094795585 wrong)`, `(2/1094795585 wrong)`, etc.

**Lesson**:

> "Stack layout is concrete. Variables sit next to each other in
> a predictable order. Without protection, a buffer overflow in
> one variable can corrupt the next one. This is the family of
> bug behind decades of security exploits."

### Part B — ASan catches the same bug (60s)

**Why this part exists**: in Part A, AddressSanitizer would *not*
catch the overflow. Both fields live inside the same struct, and
ASan tracks the struct as one block. Show the catch in a different
form — a stand-alone stack-local buffer.

**Setup**: switch the right pane to:

```bash
./demos/name_overflow_asan AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
```

**What you'll see**: a wall of red ASan output:

```
==XXXXX==ERROR: AddressSanitizer: stack-buffer-overflow on address ...
WRITE of size 31 at ...
    #0 ...in strcpy
    #1 ...in greet demos/name_overflow.c:19
    #2 ...in main demos/name_overflow.c:28
...
[32, 48) 'banner' (line 18) <== Memory access at offset 48 overflows this variable
```

**Say**:

> "Same bug — write past a 16-byte buffer with a too-long input.
> But this time the buffer is a stand-alone stack array, not a
> field inside a struct. ASan put redzones around the buffer and
> caught the overflow at exactly the line of code that did it.
> *(point at line 19)*. Two truths: tools catch most of these
> bugs, but **not all of them** — Part A slipped past ASan because
> of how its instrumentation works. Sanitizers are powerful but
> not a replacement for understanding."

**Lesson**:

> "Sanitizers and compilers protect us from many overflows, but
> they have blind spots. Defense in depth — review, sanitizers,
> and stack canaries — exists because no single tool catches
> everything."

---

## Demo 2 — Heap leak (1:30)

**Say**:

> "Now we go to the heap. The `malloc` call gives us a pointer; the
> `free` call returns the memory. If you forget to free, the memory
> stays reserved forever. That's a leak — and we're going to
> trigger one deliberately."

**Setup**: in the right pane:

```bash
rm -f /tmp/inspector.log
LEAK=1 INSPECTOR_LOG=/tmp/inspector.log valgrind --leak-check=full ./hangman
```

Play the game to the end (lose it quickly with 6 wrong guesses, or
type wrong letters fast). When it finishes, valgrind prints its
report.

**What you'll see in the inspector** (after the game ends):

The heap stays full — 12 allocations still showing. That's the
leak: nothing was freed.

**What you'll see in valgrind's output**:

```
==XXXXX== HEAP SUMMARY:
==XXXXX==     in use at exit: 219 bytes in 12 blocks
==XXXXX==     total heap usage: 16 allocs, 4 frees, 9,395 bytes allocated
==XXXXX==
==XXXXX== 9 bytes in 1 blocks are definitely lost in loss record 1 of 3
==XXXXX==    at 0x4844818: malloc
==XXXXX==    by 0x40145F: new_game (hangman.c:62)
==XXXXX==    by 0x4018BF: main (hangman.c:136)
==XXXXX==
==XXXXX== 210 (128 direct, 82 indirect) bytes in 1 blocks are definitely lost
==XXXXX==    at 0x4844818: malloc
==XXXXX==    by 0x4012BC: load_words (hangman.c:26)
==XXXXX==    by 0x401882: main (hangman.c:133)
==XXXXX==
==XXXXX== LEAK SUMMARY:
==XXXXX==    definitely lost: 137 bytes in 2 blocks
==XXXXX==    indirectly lost: 82 bytes in 10 blocks
```

**Say**:

> "Valgrind tells us exactly what's wrong. Nine bytes lost at
> `new_game` line 62 — that's the `mask` allocation. The secret
> word in this game was nine bytes including the null terminator,
> so this matches. And 210 bytes lost at `load_words` line 26 — 128
> direct bytes for the array of word pointers, plus 82 indirect
> bytes for the words those pointers point to."

> "Notice the words *directly* lost vs. *indirectly* lost. The
> array is directly lost — nothing points to it anymore. The 10
> words are indirectly lost — they're still pointed to by the
> array, but the array itself is orphaned."

**Lesson**:

> "Every `malloc` needs exactly one matching `free`. Forgetting is
> easy — humans miss it constantly. Tools like valgrind exist
> because we need them, not as a luxury."

---

## Demo 3 — Use-after-free (1:30)

**Say**:

> "The mirror image of a leak: instead of forgetting to free, we
> read memory **after** we've freed it. The pointer is still in
> our hand, but the bytes it points to are no longer ours."

**Setup**: switch to the ASan build in the right pane:

```bash
USE_AFTER_FREE=1 ./hangman_asan
```

Play the game quickly to the end.

**What you'll see** at the end: a wall of red ASan output:

```
==XXXXX==ERROR: AddressSanitizer: heap-use-after-free on address ...
READ of size 2 ...
    #0 ...in printf
    #3 ...in main /home/student/cm203-hangman/hangman.c:178

freed by thread T0 here:
    #1 ...in free_words /home/student/cm203-hangman/hangman.c:53
    #2 ...in main /home/student/cm203-hangman/hangman.c:168

previously allocated by thread T0 here:
    #1 ...in load_words /home/student/cm203-hangman/hangman.c:41
    #2 ...in main /home/student/cm203-hangman/hangman.c:133
```

**Say**:

> "ASan caught it, with three stack traces in one error. Where the
> bug fired — line 178, the printf. Where the memory was freed —
> line 53 in `free_words`. And where it was originally allocated
> — line 41 in `load_words`. Three traces let me find the bug, the
> mistake, and the policy violation in one report."

> "And here's the dangerous part. *(switch to the default build,
> run again with USE_AFTER_FREE=1.)* In the default build, the
> program might print a perfectly normal-looking word. The freed
> memory hasn't been reused yet, so the bytes are still there.
> Tests pass. The bug ships."

**Lesson**:

> "Use-after-free bugs are silent until they aren't. The freed bytes
> often remain readable, which means the program seems to work.
> Sanitizers catch what testing misses. Real-world example: this
> family of bug is behind a large fraction of memory safety CVEs
> in C and C++ programs."

---

## Demo 4 — Process memory cheat (1:30)

**Say**:

> "The last demo is a different angle. The previous three were about
> *bugs in our code*. This one is about what C **doesn't protect us
> from**: another process — or the user themselves — reading our
> memory while the game runs."

**Setup**: in the right pane, start a fresh game:

```bash
rm -f /tmp/inspector.log
INSPECTOR_LOG=/tmp/inspector.log ./hangman
```

Stop at the "enter your name" prompt — don't type yet, leave the
process running.

**Say**:

> "Hangman is now running. Its secret word is somewhere in that
> heap, but we can't see it from outside. Or can we?"

In the left pane (the inspector pane — close it temporarily, we'll
need a regular shell):

```bash
./demos/cheat_dump.sh
```

You will be prompted for sudo password.

**Say** (as you type the password):

> "Linux has a kernel security feature called Yama that prevents
> one process from reading another process's memory. This
> requires elevated privileges, so I'm typing my password.
> *(types password.)* If this were a multi-user system, only an
> admin could do this. On any single-user machine — laptop, phone,
> server — the user has it."

**What you'll see**:

```
=== Cheating against hangman (PID XXXXX) ===

Step 1 — memory map (no privileges needed):
------
XXXXXX-XXXXXX rw-p ... [heap]
XXXXXX-XXXXXX rw-p ... [stack]

Step 2 — dump process memory with gcore (needs sudo for ptrace):
------
(used sudo — ptrace_scope is restricting unprivileged ptrace)

Step 3 — strings the dump, filter for known words from words.txt:
------
banana
dolphin
elephant
guitar
keyboard
mountain
oxygen
pyramid
spaghetti
volcano
```

**Say**:

> "All ten candidate words are in the dump. Whichever one the game
> picked is right there in plain text — `malloc`'d data is just
> bytes in a process's address space. There's no encryption, no
> permission boundary inside one process. **C gives the programmer
> no built-in privacy.**"

**Lesson**:

> "When you store data in process memory, you should treat it as
> visible to anyone who can run code on the machine. Higher-level
> language runtimes don't change this — they're built on top of
> the same primitives. Real defense is process isolation, sandboxes,
> and not putting secrets in memory longer than you absolutely
> have to."

---

## Wrap (30 seconds)

**Say**:

> "Five live demonstrations, four lessons. C is faster and more
> precise than any other language **because** it doesn't hide the
> machine. That's the trade. The cost is that programmers carry
> the weight of memory safety, and tools like AddressSanitizer,
> valgrind, and stack canaries exist because we need them. The
> next time you write a `malloc`, picture this inspector. Memory
> isn't abstract. It's right there, every byte of it. Thank you."

Pause for questions.

---

## If something goes wrong on stage

- **Inspector pane is blank**: did you run with `INSPECTOR_LOG=...`?
  Check that the log file exists.
- **`make asan_demo` fails**: the `demos/` directory may not exist.
  Run `mkdir -p demos` and try again.
- **`gcore` says permission denied**: `sudo` may be timed out. Run
  `sudo -v` first to refresh the password timestamp.
- **The buffer overflow doesn't corrupt `max_wrong`**: you typed
  fewer than 17 characters, or your terminal stripped some. Type
  20 characters of the same letter to be safe.
- **Inspector shows weird offsets**: the `gs.name` field's value may
  have unprintable bytes. Don't worry — the *value* of `max_wrong`
  is the headline, not the exact bytes in `name`.
- **Game won't compile after a pull**: someone changed the Makefile.
  `git log Makefile` to see who/why; ask before changing.

---

## Q&A primers (likely questions)

**Q: Why did you turn off the stack canary?**

> Because we want to demonstrate the overflow itself, not the
> defense against it. The default `make` uses `-fno-stack-protector`
> for that reason. In any real production build, the canary would
> be on, and a long name would crash the program with "stack
> smashing detected" — exactly what the canary is for. We disabled
> it to make the bug visible.

**Q: How is your inspector different from gdb?**

> gdb is a general debugger — interactive, breakpoint-based, very
> powerful but verbose. Our inspector is single-purpose — it
> visualizes one program's memory model with cross-referenced
> pointers and field offsets, in real time, with no input needed.
> The two are complementary; the inspector is for explanation, gdb
> is for investigation.

**Q: Why didn't ASan catch the first overflow?**

> Because both buffers — `name` and `max_wrong` — live inside the
> same `GameState` struct, and ASan tracks the struct as one
> allocation. The overflow stays inside the struct's bounds. ASan's
> redzones live *between* allocations, not between fields. Part B
> of the demo shows the case where ASan does catch — when the
> buffer is a separate stack-local array and the overflow crosses
> the redzone.

**Q: Wouldn't malloc clear memory before giving it back?**

> No — `malloc` returns whatever bytes were there before. `calloc`
> zero-fills, but `malloc` doesn't. That's why the use-after-free
> demo can sometimes return correct-looking output: the freed
> bytes haven't been touched yet.

**Q: What's `0x41414141`?**

> Four bytes, each holding the ASCII value of 'A' (which is `0x41`).
> When the program reads those four bytes as a 32-bit integer
> (because that's what `int max_wrong` is), it sees the number
> 1,094,795,585. The hex makes it obvious where the bytes came
> from — letter A's, repeated.
