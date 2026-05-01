# cm203-hangman

A C hangman game used as a microscope for the C memory model.
Group project for **CM-203 Computer Organization, Project 1**.

The game itself is a Trojan horse — what we actually present is what its
memory does: stack frames, heap allocations, `.rodata` strings, padding,
and four deliberate abuses (a leak, a use-after-free, a buffer-overflow
exploit, and a "secret" recovered from process memory while the game runs).

> **Status:** game logic is working — load words, pick a secret, play with
> a 6-wrong-guess limit, win/lose. The memory inspector and the four
> abuse demos are still to come.

---

## Requirements

Linux with the standard C toolchain. Developed on Debian 13 / gcc 14, but
anything similar works.

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

You'll see a masked word, get prompted for a letter, and play until you
either reveal the word or run out of wrong guesses. Words are loaded from
`words.txt` — feel free to add your own (one per line).

## For teammates (same university LXC setup)

This assumes you're on the CM-203 student LXC (Debian 13) and have already
SSH'd into it from VS Code (Remote-SSH) the same way as for the other
class exercises. Steps:

### 1. One-time SSH key setup for GitHub

If you've never pushed to GitHub from this LXC before:

```bash
# Identify yourself in git
git config --global user.name "your-name"
git config --global user.email "your-email@example.com"

# Generate an ed25519 key (press Enter at every prompt)
ssh-keygen -t ed25519 -C "your-email@example.com"

# Print the public key, copy the whole line
cat ~/.ssh/id_ed25519.pub
```

Take the line that starts with `ssh-ed25519 ...` and add it on GitHub:
**Settings → SSH and GPG keys → New SSH key**. Title it whatever you
recognize ("Uni LXC student container" works).

Then test:

```bash
ssh -T git@github.com
```

You should see "Hi <your-username>! You've successfully authenticated...".

### 2. Clone the project

```bash
cd ~
git clone git@github.com:zeltmanis/cm203-hangman.git
cd cm203-hangman
```

### 3. Install missing tools (one-time)

The LXC ships with `gcc` and `make`. Add the rest:

```bash
sudo apt install -y gdb valgrind
```

### 4. Build and run

```bash
make
./hangman
```

You should see the masked word and a guess prompt.

### 5. Pulling teammates' changes

Whenever someone else pushes:

```bash
git pull
make            # rebuild after any code change
```

### 6. Pushing your own changes

```bash
git status                      # see what changed
git add <files-you-want>        # stage specific files
git commit -m "what you did"
git push
```

> Don't `git add .` blindly — your local `hangman` and `hangman_asan`
> binaries are gitignored, but it's still cleaner to add by name.

## Project layout

```
.
├── Makefile        # build rules for the demo and ASan variants
├── hangman.c       # the game (currently: word load, mask, guess loop, GameState struct)
├── words.txt       # word list, one per line, loaded at runtime
├── .gitignore      # ignores compiled binaries and editor junk
└── README.md       # this file
```

Files we plan to add:

- `inspector.py` — Python-side memory inspector for the right tmux pane
- `demos/` — small shell scripts that drive each of the four abuses
- `notes/` — study notes mapped to "The C Book" chapters

## Presentation plan

*(To be filled in as we build. Outline: Option A architecture — left tmux
pane runs the game, right pane runs the memory inspector reading events
emitted by the game. Four abuses, ~2 minutes each, three students splitting
ownership of game / inspector / demo scripts.)*

## Study notes

*(To be filled in. Topics mapped to "The C Book" chapters: arrays & pointers,
structs, dynamic memory, function pointers, and the security/tooling pieces
that come from outside the book — AddressSanitizer, valgrind, `/proc/PID/maps`.)*
