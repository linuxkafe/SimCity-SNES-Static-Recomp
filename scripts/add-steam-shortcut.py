#!/usr/bin/env python3
"""
add-steam-shortcut.py — add a non-Steam game to Steam's shortcuts.vdf.

Why this is written in Python and not with text tools
-----------------------------------------------------
shortcuts.vdf is Steam's binary KeyValues format: strings are length-prefixed
and separated by NUL bytes, and the file on a real Deck is over 200 KB of the
user's own library.  Appending text to it would corrupt every existing entry.
This reads the file as bytes, parses it properly, and writes it back.

It also refuses rather than guesses:

  * a Steam that is running rewrites this file on exit, so the entry would be
    lost -- the installer checks for that before calling;
  * an entry for the same executable already present is not duplicated;
  * a file that does not parse is left completely untouched, and reported.

Environment:
  SIMCITY_VDF    path to shortcuts.vdf
  SIMCITY_NAME   name shown in the Steam library
  SIMCITY_EXE    executable to launch
  SIMCITY_START  working directory
"""

import os
import shutil
import sys
import time

TYPE_MAP = {}


def cstr(s):
    """A KeyValues string: length-prefixed, NUL-terminated, bytes."""
    b = s.encode("utf-8") + b"\x00"
    return bytes([len(b)]) + b


def key(name):
    return cstr(name)


def kv_end():
    return b"\x08"


def parse(data):
    """Parse the binary KeyValues tree (Steam shortcuts.vdf format)."""
    pos = 0

    def read_len():
        nonlocal pos
        n = data[pos]
        pos += 1
        s = data[pos:pos + n - 1].decode("utf-8", "replace")
        pos += n
        return s

    def read_bare():
        nonlocal pos
        end = data.index(b"\x00", pos)
        s = data[pos:end].decode("utf-8", "replace")
        pos = end + 1
        return s

    def parse_node():
        nonlocal pos
        if pos >= len(data):
            return None
        t = data[pos]
        pos += 1
        if t == 0x00:
            # Bare name label
            name = read_bare()
            return (0, name, None)
        if t == 0x08:
            return (8, None, None)
        if t == 0x01:  # string
            name = read_len()
            val = read_len()
            return (1, name, val)
        if t == 0x02:  # object (anonymous)
            children = []
            while pos < len(data) and data[pos] != 0x08:
                field_type = data[pos]
                pos += 1
                if pos >= len(data):
                    break
                # Field name is BARE (NUL-terminated)
                fname_end = data.index(b"\x00", pos)
                fname = data[pos:fname_end].decode("utf-8", "replace")
                pos = fname_end + 1
                # Value type
                if pos >= len(data):
                    break
                vtype = data[pos]
                pos += 1
                if vtype == 0x01:
                    # String value: length-prefixed
                    val = read_len()
                elif vtype == 0x03:
                    # Int32
                    val = int.from_bytes(data[pos:pos+4], "little", signed=True)
                    pos += 4
                else:
                    val = None
                children.append((fname, vtype, val))
            pos += 1  # skip 0x08
            return (2, None, children)
        if t == 0x03:
            v = int.from_bytes(data[pos:pos+4], "little", signed=True)
            pos += 4
            return (3, None, v)
        if t == 0x08:
            return (8, None, None)
        # Skip unknown
        return None

    # The file is a flat sequence of nodes: the first is the "shortcuts"
    # header (type 0x00, bare name "shortcuts"), then alternating label
    # nodes (type 0x00, bare name = index) and entry objects (type 0x02).
    nodes = []
    while pos < len(data):
        node = parse_node()
        if node is None:
            break
        nodes.append(node)
        if len(nodes) > 10000:  # safety
            break

    if not nodes or nodes[0][1] != "shortcuts":
        raise ValueError("raiz inesperada: %r" % (nodes[0][1] if nodes else None))
    # Nodes: [header, label0, entry0, label1, entry1, ...]
    # Pair up label nodes (type 0) with their object nodes (type 2)
    entries = []
    i = 1  # skip header
    while i < len(nodes):
        if nodes[i][0] == 0:  # label
            if i + 1 < len(nodes) and nodes[i+1][0] == 2:
                entries.append(nodes[i+1])
                i += 2
            else:
                i += 1
        else:
            i += 1
    return (2, "shortcuts", entries)


def build(node):
    """Serialise back to the same tag scheme."""
    t, name, payload = node
    if t == 2:  # object
        out = b"\x02"
        for c in payload:
            out += build(c)
        return out + b"\x08"
    if t == 1:  # string
        return b"\x01" + key(payload) + cstr(payload)
    if t == 3:
        return b"\x03" + key(payload) + str(int(payload)).encode() + b"\x00"
    if t == 0:
        return b"\x00" + cstr(payload)
    return bytes([t]) + key(payload)


def cstr(s):
    b = s.encode("utf-8") + b"\x00"
    return bytes([len(b)]) + b


def key(name):
    return cstr(name)


def kv_end():
    return b"\x08"


def find_entries(node):
    """Yield (index, node) for each shortcut entry."""
    t, name, payload = node
    if t != 2 or not payload:
        return
    entries = [c for c in payload if c[0] == 2]
    for i, e in enumerate(entries):
        yield i, e


def entry_field(entry, field):
    for c in entry[2] or []:
        if c[0] == field:
            return c[2]
    return None


def main():
    vdf = os.environ.get("SIMCITY_VDF")
    name = os.environ.get("SIMCITY_NAME", "SimCity SNES")
    exe = os.environ.get("SIMCITY_EXE")
    start = os.environ.get("SIMCITY_START", "")
    if not vdf or not exe:
        print("SIMCITY_VDF e SIMCITY_EXE sao obrigatorios", file=sys.stderr)
        return 2
    if not os.path.isfile(vdf):
        open(vdf, "wb").close()

    with open(vdf, "rb") as f:
        data = f.read()

    try:
        root = parse(data)
    except Exception as e:
        print("nao consegui interpretar %s: %s" % (vdf, e), file=sys.stderr)
        print("o ficheiro nao foi alterado", file=sys.stderr)
        return 1

    # Never add a second copy of the same launcher.
    for _, entry in find_entries(root):
        if entry_field(entry, "Exe") == exe:
            print("ja existe um atalho para %s" % exe, file=sys.stderr)
            return 0

    entries = list(find_entries(root))
    index = str(len(entries))

    # Build the new entry node
    node = (2, None, [
        (1, "AppName", name),
        (1, "Exe", '"%s"' % exe),
        (1, "StartDir", '"%s"' % start),
    ])

    # Find the root node and append the new entry
    # root is (2, "shortcuts", entries)
    root_entries = root[2]
    root_entries.append((2, None, [
        (1, "AppName", name),
        (1, "Exe", '"%s"' % exe),
        (1, "StartDir", '"%s"' % start),
    ]))

    backup = "%s.backup-%s" % (vdf, time.strftime("%Y%m%d-%H%M%S"))
    shutil.copy2(vdf, backup)

    out = build((2, "shortcuts", root_entries))

    # Verify what we are about to write still parses and still has everything.
    check = parse(out)
    before = len([n for n in parse(root) if n and n[0] == 2]) - 1  # minus header
    after = list(find_entries(check))
    if len(after) != before + 1:
        print("a verificacao falhou: %d entradas antes, %d depois" % (before, len(after)),
              file=sys.stderr)
        print("o ficheiro nao foi alterado", file=sys.stderr)
        return 1
    if entry_field((2, None, after[-1][2]), "Exe") != '"%s"' % exe:
        print("a verificacao do Exe falhou; o ficheiro nao foi alterado", file=sys.stderr)
        return 1

    with open(vdf, "wb") as f:
        f.write(out)
    print("atalho '%s' adicionado" % name)
    return 0


if __name__ == "__main__":
    sys.exit(main())