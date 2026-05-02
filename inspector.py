#!/usr/bin/env python3
"""Live memory inspector for hangman.

Reads memory events emitted by the C game and renders a live ASCII view
of heap and stack. Run this in a tmux pane next to the game terminal:

    # in pane 1:
    INSPECTOR_LOG=/tmp/inspector.log ./hangman

    # in pane 2:
    ./inspector.py /tmp/inspector.log

The inspector polls the log file and redraws the screen whenever the
file grows.
"""

import os
import sys
import time


def parse_field(rest):
    """Parse 'parent=X offset=N size=N name=NAME value=VALUE' into a dict.

    The value may contain spaces and is treated as everything after 'value='.
    """
    out = {}
    keys = ["parent", "offset", "size", "name", "value"]
    s = rest
    for i, key in enumerate(keys):
        prefix = key + "="
        idx = s.find(prefix)
        if idx < 0:
            continue
        s = s[idx + len(prefix):]
        if i + 1 < len(keys):
            next_prefix = " " + keys[i + 1] + "="
            end = s.find(next_prefix)
            if end >= 0:
                out[key] = s[:end]
                s = s[end + 1:]
            else:
                out[key] = s
        else:
            out[key] = s
    return out


class Model:
    def __init__(self):
        self.heap = {}     # addr -> {"size", "label", "value"}
        self.stack = {}    # addr -> {"size", "label", "fields": [...]}
        self.notes = []    # most recent NOTE messages
        self.events = []   # last N raw events for the events pane

    def feed(self, line):
        line = line.rstrip("\n")
        if not line:
            return
        self.events.append(line)
        if len(self.events) > 6:
            self.events.pop(0)

        parts = line.split(" ", 1)
        typ = parts[0]
        rest = parts[1] if len(parts) > 1 else ""

        if typ == "ALLOC":
            tokens = rest.split(" ", 2)
            if len(tokens) >= 3:
                addr, size, label = tokens
                self.heap[addr] = {"size": int(size), "label": label, "value": None}
        elif typ == "FREE":
            addr = rest.strip()
            self.heap.pop(addr, None)
        elif typ == "WRITE":
            tokens = rest.split(" ", 1)
            if len(tokens) == 2:
                addr, value = tokens
                value = value.strip().strip('"')
                if addr in self.heap:
                    self.heap[addr]["value"] = value
        elif typ == "STACK":
            tokens = rest.split(" ", 2)
            if len(tokens) >= 3:
                addr, size, label = tokens
                if addr not in self.stack:
                    self.stack[addr] = {"size": int(size), "label": label, "fields": []}
        elif typ == "FIELD":
            f = parse_field(rest)
            parent = f.get("parent")
            if parent in self.stack:
                fields = [x for x in self.stack[parent]["fields"]
                          if x["name"] != f.get("name")]
                fields.append({
                    "offset": int(f.get("offset", "0")),
                    "size": int(f.get("size", "0")),
                    "name": f.get("name", ""),
                    "value": f.get("value", "").strip(),
                })
                fields.sort(key=lambda x: x["offset"])
                self.stack[parent]["fields"] = fields
        elif typ == "NOTE":
            self.notes.append(rest)


def annotate(value, heap):
    """If value looks like a heap pointer, append the heap label."""
    if value.startswith("0x") and value in heap:
        return f"{value}  -> heap [{heap[value]['label']}]"
    return value


def render(model):
    width = 72
    bar = "=" * width
    sep = "-" * width
    out = []

    out.append(bar)
    out.append(f"  HEAP   ({len(model.heap)} live allocations)")
    out.append(bar)
    if not model.heap:
        out.append("  (empty)")
    else:
        for addr in sorted(model.heap.keys()):
            entry = model.heap[addr]
            v = entry["value"]
            v_str = f'  = "{v}"' if v is not None else ""
            out.append(f"  {addr}  [{entry['size']:>4} bytes]  {entry['label']:<14}{v_str}")
    out.append("")

    out.append(bar)
    out.append(f"  STACK")
    out.append(bar)
    if not model.stack:
        out.append("  (empty)")
    else:
        for addr in sorted(model.stack.keys()):
            entry = model.stack[addr]
            out.append(f"  {addr}  [{entry['size']:>4} bytes]  {entry['label']}")
            out.append(sep)
            for field in entry["fields"]:
                v = annotate(field["value"], model.heap)
                out.append(
                    f"    +{field['offset']:<3} {field['name']:<10} "
                    f"({field['size']:>2}b)  = {v}"
                )
    out.append("")

    out.append(bar)
    out.append("  RECENT EVENTS")
    out.append(bar)
    for e in model.events[-6:]:
        out.append(f"  {e}")

    return "\n".join(out)


def replay_log(path):
    model = Model()
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                model.feed(line)
    return model


def main():
    if len(sys.argv) != 2:
        print("usage: inspector.py <event-log-path>", file=sys.stderr)
        sys.exit(1)

    path = sys.argv[1]
    last_size = -1
    while True:
        try:
            size = os.path.getsize(path) if os.path.exists(path) else 0
        except OSError:
            size = 0

        if size != last_size:
            model = replay_log(path)
            sys.stdout.write("\033[2J\033[H")  # clear screen, cursor home
            sys.stdout.write(render(model))
            sys.stdout.write("\n")
            sys.stdout.flush()
            last_size = size

        try:
            time.sleep(0.2)
        except KeyboardInterrupt:
            break


if __name__ == "__main__":
    main()
