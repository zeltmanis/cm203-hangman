# cm203-hangman

A C hangman game used as a microscope for the C memory model.
Group project for **CM-203 Computer Organization, Project 1**.

The game itself is a Trojan horse — what we actually present is what its
memory does: stack frames, heap allocations, `.rodata` strings, padding,
and four deliberate abuses (a leak, a use-after-free, a buffer-overflow
exploit, and a "secret" recovered from process memory while the game runs).

> **Status:** skeleton only — build pipeline verified. The game logic, the
> memory inspector, and the demo scripts are still to be built.

---

## Requirements

Linux (developed on Debian 13 / gcc 14). The toolchain:

- `gcc` (any recent version with `-fsanitize=address` support)
- `make`
- `gdb`
- `valgrind`

Install on Debian / Ubuntu:

```bash
sudo apt install -y build-essential gdb valgrind
```

## Build

```bash
make           # default build — deliberately vulnerable, used for the demo
make asan      # AddressSanitizer + UBSan build, used to show the catch
make clean     # remove built binaries
```

The default build uses `-O0 -fno-stack-protector -no-pie` on purpose: this
keeps the stack layout predictable for the buffer-overflow demo and disables
the stack canary so the overflow actually flips the adjacent variable instead
of being caught at runtime. We explain *why* those protections exist (and
what we turned off) during the presentation — that's part of the lesson.

## Run

```bash
./hangman
```

Currently this just prints a banner — the game logic isn't wired up yet.

## Project layout

```
.
├── Makefile        # build rules for the demo and ASan variants
├── hangman.c       # the game itself (skeleton at this stage)
├── .gitignore      # ignores compiled binaries and editor junk
└── README.md       # this file
```

Files we plan to add:

- `words.txt` — the word list loaded at runtime (forces real heap usage)
- `inspector.py` — Python-side memory inspector for the right tmux pane
- `demos/` — small shell scripts that drive each of the four abuses
- `notes/` — study notes mapped to "The C Book" chapters

## For teammates

Clone:

```bash
git clone git@github.com:zeltmanis/cm203-hangman.git
cd cm203-hangman
```

You'll need an SSH key linked to your GitHub account. From your LXC:

```bash
ssh-keygen -t ed25519 -C "your-email@example.com"
cat ~/.ssh/id_ed25519.pub
# Paste that line into https://github.com/settings/keys → New SSH key
ssh -T git@github.com    # should greet you by name
```

Then build and run:

```bash
make && ./hangman
```

## Presentation plan

*(To be filled in as we build. Outline: Option A architecture — left tmux
pane runs the game, right pane runs the memory inspector reading events
emitted by the game. Four abuses, ~2 minutes each, three students splitting
ownership of game / inspector / demo scripts.)*

## Study notes

*(To be filled in. Topics mapped to "The C Book" chapters: arrays & pointers,
structs, dynamic memory, function pointers, and the security/tooling pieces
that come from outside the book — AddressSanitizer, valgrind, `/proc/PID/maps`.)*
