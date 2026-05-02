# cm203-hangman

A C hangman game used as a microscope for the C memory model.
Group project for **CM-203 Computer Organization, Project 1**.

The game itself is a Trojan horse — what we actually present is what its
memory does: stack frames, heap allocations, struct padding, and four
deliberate abuses (a leak, a use-after-free, a buffer-overflow exploit,
and a "secret" recovered from process memory while the game runs). A
custom Python inspector visualizes the running program's memory live in
a side-by-side terminal pane.

> **Status:** game logic + 4 demos + memory inspector all working. Repo
> is presentation-ready; the remaining work is rehearsal.

---

## Quick start

```bash
make                                          # build the game
./hangman                                     # play it
```

For the live memory view (recommended — this is the project's centerpiece):

```bash
# pane 1 (left): the inspector
./inspector.py /tmp/inspector.log

# pane 2 (right): the game, instrumented
rm -f /tmp/inspector.log
INSPECTOR_LOG=/tmp/inspector.log ./hangman
```

The inspector polls the log file and redraws on every event. As you
play, the heap and stack views update live.

---

## Documentation

- **[notes/MEMORY_WALKTHROUGH.md](notes/MEMORY_WALKTHROUGH.md)** —
  team onboarding doc: what memory is, stack vs heap, the C code
  line by line, the demos in plain language, and a reading map for
  *The C Book*.
- **[notes/PRESENTATION_DEMOS.md](notes/PRESENTATION_DEMOS.md)** —
  the 8-minute presentation script: exact commands to type, what to
  expect on each pane, narration to say, lesson per demo.

Read the walkthrough first, then the demo script.

---

## Requirements

Linux with the standard C toolchain. Developed on Debian 13 / gcc 14
on the university LXC, but anything similar works.

- `gcc` (any recent version with `-fsanitize=address` support)
- `make`
- `gdb`
- `valgrind`
- `python3` (for the inspector — no extra packages needed)

Install missing tools on Debian / Ubuntu:

```bash
sudo apt install -y build-essential gdb valgrind python3
```

## Build targets

```bash
make            # default build — vulnerable, used for the demos
make asan       # ASan + UBSan build of the game (./hangman_asan)
make asan_demo  # companion stack-overflow demo (./demos/name_overflow_asan)
make clean      # remove all built binaries
```

The default build uses `-O0 -fno-stack-protector -no-pie` on purpose
— it keeps the stack layout predictable for the buffer-overflow demo
and disables the canary that would otherwise catch the overflow. We
explain *why* those protections exist and what we turned off during
the presentation.

## Running the demos

Each demo is one short command. See `notes/PRESENTATION_DEMOS.md` for
exact talking points and what to expect on screen.

| # | Demo | Command |
|---|---|---|
| 1A | Buffer overflow → infinite lives (ASan misses) | `INSPECTOR_LOG=/tmp/inspector.log ./hangman` then type a 20+ character name |
| 1B | Same overflow, ASan catches it | `make asan_demo && ./demos/name_overflow_asan AAAAAAAAAAAAAAAAAAAA` |
| 2 | Heap leak | `LEAK=1 valgrind --leak-check=full ./hangman` |
| 3 | Use-after-free | `USE_AFTER_FREE=1 ./hangman_asan` |
| 4 | Secret leak via process memory | start game in one pane, run `./demos/cheat_dump.sh` in another |

### Environment variables

The game reacts to these:

- `INSPECTOR_LOG=<path>` — emit memory events to this path. The
  Python inspector reads it. Without this, the game runs identically
  to a normal build (no extra output, no slowdown).
- `LEAK=1` — skip the cleanup at end of game. Use under valgrind to
  see the leak report.
- `USE_AFTER_FREE=1` — read `gs.secret` after `free_words` has run.
  Use with the ASan build for a clear catch.

---

## For teammates (same university LXC setup)

This assumes you're on the CM-203 student LXC (Debian 13) with VS Code
Remote-SSH already working.

### 1. One-time SSH key setup for GitHub

If you've never pushed to GitHub from this LXC before:

```bash
git config --global user.name "your-name"
git config --global user.email "your-email@example.com"

ssh-keygen -t ed25519 -C "your-email@example.com"   # press Enter at every prompt
cat ~/.ssh/id_ed25519.pub                            # copy the line
```

Add the public key on GitHub: **Settings → SSH and GPG keys → New SSH
key**. Then test:

```bash
ssh -T git@github.com    # should greet you by name
```

### 2. Clone the repo

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

### 5. Side-by-side inspector

In VS Code, click the "split terminal" icon (top-right of the
terminal panel) to get two panes. Then:

```bash
# left pane:
./inspector.py /tmp/inspector.log

# right pane:
rm -f /tmp/inspector.log
INSPECTOR_LOG=/tmp/inspector.log ./hangman
```

### 6. Pulling teammates' changes

```bash
git pull
make             # rebuild after any change to .c files
```

### 7. Pushing your own changes

```bash
git status
git add <files-by-name>
git commit -m "what you did"
git push
```

> Don't `git add .` blindly — the local `hangman` and demo binaries
> are gitignored, but it's still cleaner to add by name.

---

## Project layout

```
.
├── Makefile                       # build rules for the game and the demos
├── hangman.c                      # the game (game loop + inspector hooks)
├── inspector.py                   # Python memory inspector (the right-pane view)
├── words.txt                      # word list, one per line, loaded at runtime
├── demos/
│   ├── name_overflow.c            # companion overflow demo (ASan catches)
│   └── cheat_dump.sh              # extract secret from running game's memory
├── notes/
│   ├── MEMORY_WALKTHROUGH.md      # team onboarding: memory model, code, C Book map
│   └── PRESENTATION_DEMOS.md      # 8-minute presentation script
├── .gitignore
└── README.md                      # this file
```

## License

This is a student project for CM-203. No license — please don't
copy-paste it for your own coursework.
