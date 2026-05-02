#!/bin/bash
#
# Demonstrate that hangman's secret word lives in process memory and can
# be recovered by anyone who can ptrace the process. Run this in a
# separate terminal while ./hangman is playing.
#
# Steps:
#   1. Show the process's memory map (no privileges needed).
#   2. Dump the process memory with gcore (needs sudo for ptrace, OR
#      ptrace_scope lowered to 0).
#   3. Run `strings` on the dump and filter for known words.

set -e

# Work from the project root regardless of where we were called from.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

PID=$(pgrep -n hangman || true)
if [ -z "$PID" ]; then
    echo "No hangman process found. Start ./hangman in another terminal first." >&2
    exit 1
fi

echo "=== Cheating against hangman (PID $PID) ==="
echo

echo "Step 1 — memory map (no privileges needed):"
echo "------"
grep -E "heap|stack" /proc/$PID/maps
echo
echo "Heap region exists. The secret was malloc'd into it by load_words()."
echo

# Try gcore without sudo first (works if ptrace_scope=0 or we are privileged).
# Fall back to sudo gcore otherwise.
echo "Step 2 — dump process memory with gcore:"
echo "------"
if gcore -o /tmp/cheat $PID >/dev/null 2>&1; then
    echo "(no sudo needed)"
elif sudo gcore -o /tmp/cheat $PID >/dev/null 2>&1; then
    echo "(used sudo — ptrace_scope is restricting unprivileged ptrace)"
else
    echo "gcore failed. Either ptrace is restricted or sudo isn't available." >&2
    exit 1
fi
echo "Dump written to /tmp/cheat.$PID"
echo

DUMP=/tmp/cheat.$PID
echo "Step 3 — strings the dump, filter for known words from words.txt:"
echo "------"
strings "$DUMP" | grep -wFf words.txt | sort -u
echo

echo "All loaded words are in the dump. The secret is one of them."
echo "(Cleaning up: $DUMP)"
sudo rm -f "$DUMP" 2>/dev/null || rm -f "$DUMP"
