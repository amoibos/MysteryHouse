#!/usr/bin/env python3
"""Byte pair encoding of the game messages (build step).

Copies the given C sources to an output directory with every string literal
replaced by its compressed form, and writes bpe_table.c with the pair table.
The sources themselves keep plain, readable messages.

    compressed literal  "\\x01" + codes      (marker byte BPE_MARK)
    code < 0x80         the character itself
    code >= 0x80        pair bpe_pairs[code - 0x80] (two codes, recursively)

Only literals that end up in transcribe() may be packed; files and arrays
are therefore selected explicitly: whole files, or only the initialisers of
the arrays named with --arrays (gamedata.c: room_descr, obj_descr).

usage: pack_texts.py --out DIR [--arrays a,b] file.c ... [--only-arrays file.c ...]
"""

import argparse
import collections
import os
import re

BPE_MARK = 0x01
FIRST_CODE = 0x80
MAX_DEPTH = 12          # decoder stack in bpe.c holds MAX_DEPTH + 2 entries

# string literal or comment / char literal (skipped, kept as is)
TOKEN = re.compile(r'"(?:[^"\\\n]|\\.)*"|/\*.*?\*/|//[^\n]*|\'(?:[^\'\\\n]|\\.)*\'', re.S)


def c_unescape(body):
    return bytes(body, "latin-1").decode("unicode_escape").encode("latin-1")


def literals(src, regions):
    """(start, end, text) of string literals inside regions (None = whole file)"""
    out = []
    for m in TOKEN.finditer(src):
        tok = m.group(0)
        if not tok.startswith('"'):
            continue
        if regions is not None and not any(a <= m.start() < b for a, b in regions):
            continue
        line_start = src.rfind("\n", 0, m.start()) + 1
        if src[line_start:m.start()].lstrip().startswith("#"):
            continue                        # #include "..."
        out.append((m.start(), m.end(), c_unescape(tok[1:-1])))
    return out


def array_regions(src, names):
    regions = []
    for name in names:
        m = re.search(r"\b%s\s*\[[^\]]*\]\s*=\s*\{" % re.escape(name), src)
        if not m:
            raise SystemExit("array %s not found" % name)
        regions.append((m.end(), src.index("};", m.end())))
    return regions


def build_pairs(texts):
    seqs = [list(t) for t in texts if t]
    pairs, depth = [], {c: 0 for s in seqs for c in s}
    while FIRST_CODE + len(pairs) <= 0xFF:
        count = collections.Counter()
        for s in seqs:
            for a, b in zip(s, s[1:]):
                if max(depth[a], depth[b]) < MAX_DEPTH:
                    count[(a, b)] += 1
        if not count:
            break
        (a, b), n = count.most_common(1)[0]
        if n < 3:
            break
        code = FIRST_CODE + len(pairs)
        pairs.append((a, b))
        depth[code] = max(depth[a], depth[b]) + 1
        for s in seqs:
            i = 0
            while i < len(s) - 1:
                if s[i] == a and s[i + 1] == b:
                    s[i:i + 2] = [code]
                i += 1
    return pairs


def compress(text, pairs):
    s = list(text)
    for i, (a, b) in enumerate(pairs):
        code = FIRST_CODE + i
        j = 0
        while j < len(s) - 1:
            if s[j] == a and s[j + 1] == b:
                s[j:j + 2] = [code]
            j += 1
    return bytes([BPE_MARK] + s)


def expand(data, pairs):
    out = bytearray()

    def put(c):
        if c >= FIRST_CODE:
            put(pairs[c - FIRST_CODE][0])
            put(pairs[c - FIRST_CODE][1])
        else:
            out.append(c)
    for c in data[1:]:
        put(c)
    return bytes(out)


def c_literal(data):
    return '"' + "".join("\\x%02x" % b for b in data) + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--arrays", default="")
    ap.add_argument("files", nargs="+")
    args = ap.parse_args()
    arrays = [a for a in args.arrays.split(",") if a]

    sources = {}
    for path in args.files:
        with open(path, encoding="latin-1") as f:
            src = f.read()
        regions = array_regions(src, arrays) if os.path.basename(path) == "gamedata.c" else None
        sources[path] = (src, literals(src, regions))

    texts = [t for _, lits in sources.values() for _, _, t in lits]
    for t in texts:
        assert all(0x02 <= c < 0x80 for c in t), t
    pairs = build_pairs(texts)

    os.makedirs(args.out, exist_ok=True)
    raw = packed = 0
    for path, (src, lits) in sources.items():
        out, pos = [], 0
        for start, end, text in lits:
            data = compress(text, pairs)
            assert expand(data, pairs) == text
            out.append(src[pos:start])
            out.append(c_literal(data) if text else '""')
            pos = end
            raw += len(text) + 1
            packed += len(data) + 1
        out.append(src[pos:])
        with open(os.path.join(args.out, os.path.basename(path)), "w",
                  encoding="latin-1", newline="\n") as f:
            f.write("/* packed by tools/pack_texts.py from %s - do not edit */\n" % path)
            f.write("".join(out))

    with open(os.path.join(args.out, "bpe_table.c"), "w", newline="\n") as f:
        f.write("/* generated by tools/pack_texts.py - do not edit */\n")
        f.write('#include "bpe.h"\n\n')
        f.write("const unsigned char bpe_pairs[%d][2] = {\n" % max(1, len(pairs)))
        for a, b in pairs or [(0, 0)]:
            f.write("    {0x%02x, 0x%02x},\n" % (a, b))
        f.write("};\n")
    print("texts: %d bytes -> %d + table %d" % (raw, packed, 2 * len(pairs)))


if __name__ == "__main__":
    main()
