#!/usr/bin/env python3
"""Pictures and title screen from the Apple II original of Mystery House.

The disk image (DOS 3.3, apple2/*.dsk) holds the Versawriter pictures of
the 1980 release in full screen resolution:

  BLOCK DIRECTORY   3 bytes per picture: block file number, offset (LE)
  BLOCK1..BLOCK19   pictures: "00 00" starts a polyline, then (x, y) byte
                    pairs, "FF FF" ends the picture; 97 pictures
  STARTUP           object table: [picture] [type] [x] [y] ...; type 00 =
                    vector picture at screen (x, y), type 01 = text label
  AUTO LOAD OBJ     loaded at 0x0FFD, contains hi-res page 1 = title screen
                    (stored ZX7 compressed, see tools/zx7.py)

The game keeps the picture numbers of the Pico-8 remake (PIC_* / SPR_*);
COMPOSE below says which Apple pictures make up each of them.  Room
pictures are drawn at the screen origin, objects at twice their Pico-8
position plus a small correction taken from the Apple object table.
"""

import os

import zx7

DISK = os.path.join("apple2", "mystery_house.dsk")     # "Mistery House (1980)(Sierra)(PD)"

# -- DOS 3.3 -----------------------------------------------------------------

def read_disk(path):
    with open(path, "rb") as f:
        img = f.read()

    def sector(t, s):
        o = (t * 16 + s) * 256
        return img[o:o + 256]

    vtoc = sector(17, 0)
    t, s = vtoc[1], vtoc[2]
    files = {}
    while t:
        cat = sector(t, s)
        for i in range(7):
            e = cat[11 + i * 35:11 + i * 35 + 35]
            if e[0] in (0, 0xFF):
                continue
            name = bytes(b & 0x7F for b in e[3:33]).decode().rstrip()
            ftype, ts_t, ts_s = e[2] & 0x7F, e[0], e[1]
            data = bytearray()
            while ts_t:
                ts = sector(ts_t, ts_s)
                for k in range(122):
                    dt, ds = ts[12 + 2 * k], ts[13 + 2 * k]
                    if dt or ds:
                        data += sector(dt, ds)
                ts_t, ts_s = ts[1], ts[2]
            if ftype == 4:                      # binary: load address, length
                length = data[2] | data[3] << 8
                data = data[4:4 + length]
            files[name] = bytes(data)
        t, s = cat[1], cat[2]
    return files


def apple_pictures(files):
    directory = files["BLOCK DIRECTORY"]
    pics = []
    i = 3
    while i + 3 <= len(directory) and 1 <= directory[i] <= 19:
        block = files["BLOCK%d" % directory[i]]
        p = directory[i + 1] | directory[i + 2] << 8
        lines, cur = [], None
        while p + 1 < len(block):
            x, y = block[p], block[p + 1]
            p += 2
            if x == 0xFF and y == 0xFF:
                break
            if x == 0 and y == 0:
                cur = []
                lines.append(cur)
                continue
            if cur is None:
                cur = []
                lines.append(cur)
            cur.append((x, y))
        pics.append([l for l in lines if l])
        i += 3
    return pics                                  # index 0 = Apple picture 1

# -- Pico-8 picture number -> Apple pictures ----------------------------------

# rooms and temp rooms: Apple picture, or (picture, dx, dy) for parts that
# Pico-8 painted into the room (the dead bodies; offsets found by matching)
ROOMS = {
    1: [3], 2: [1], 3: [2], 4: [53], 5: [4], 6: [5], 7: [20], 8: [48],
    9: [24], 10: [36], 11: [51], 12: [13], 13: [14], 16: [12], 19: [11],
    22: [10], 23: [19], 24: [6], 25: [7], 26: [34], 27: [42],
    28: [15], 29: [16],                         # backyard, see BACKYARD
    30: [], 31: [],                             # fence: part of BACKYARD
    32: [30, (72, 36, 64)],                     # basement with tom
    33: [45, (81, 70, 64)],                     # bathroom with bill
    34: [35], 35: [26], 36: [27], 38: [50], 39: [46], 40: [28], 41: [29],
    44: [33], 46: [49], 47: [32], 48: [38],
    50: [37, (77, 96, 58)],                     # nursery with dr. green
    51: [47], 52: [25], 53: [23], 54: [8], 55: [9],
    56: [39, (76, 70, 70)],                     # small bedroom with sally
    57: [43], 58: [40], 59: [41],
    60: [44, (89, 166, 62)],                    # tower with daisy
    61: [22], 62: [21], 63: [31],
}

# back yard: Pico-8 combines house door and fence gate, Apple has all four
BACKYARD_FIRST = 92
BACKYARD = {92: 15, 93: 18, 94: 16, 95: 17}     # door closed/open x gate closed/open

# objects: Apple picture, Pico-8 draw position, Apple screen position
OBJECTS = {
    14: (55, (2, 39), (3, 77)),     15: (97, (2, 39), (3, 77)),     # cabinet
    17: (93, (78, 9), (155, 17)),   18: (94, (78, 9), (155, 17)),   # refrigerator
    20: (56, (7, 39), (13, 82)),    21: (57, (4, 40), (7, 83)),     # bricks, hole
    37: (68, (3, 42), (6, 83)),                                     # gravedigger
    42: (63, (40, 46), (81, 92)),   43: (64, (40, 57), (79, 115)),  # fire, hole
    45: (88, (52, 30), (104, 61)),                                  # gun
    49: (96, (36, 17), (73, 35)),                                   # dagger line
    64: (84, (100, 22), (200, 45)),                                 # trapdoor open
    65: (52, (20, 30), (41, 61)),   66: (54, (43, 33), (86, 66)),   # man, woman
    70: (62, (51, 37), (102, 75)),  71: (58, (37, 53), (75, 107)),  # candle, pitcher
    73: (60, (63, 33), (125, 66)),  74: (65, (54, 30), (107, 59)),  # butterknife, key
    75: (66, (63, 43), (125, 89)),                                  # sam dead
    76: (69, (13, 45), (27, 88)),   77: (70, (13, 44), (25, 88)),   # joe dots / x's
    78: (95, (85, 43), (171, 86)),  79: (82, (63, 12), (127, 24)),  # dagger, towel
    80: (74, (78, 10), None),       81: (75, (78, 10), None),       # brick, jewels
    82: (83, (100, 22), (200, 45)), 83: (79, (32, 5), (63, 10)),    # trapdoor, picture
    84: (80, (51, 13), (103, 27)),                                  # button
    85: (86, (47, 22), (93, 45)),   86: (87, (47, 22), (93, 45)),   # trunk
    87: (73, (85, 41), (175, 83)),  88: (85, (51, 62), (102, 124)), # skeleton key, hammer
    89: (90, (102, 34), (203, 65)), 90: (91, (101, 34), (201, 65)), # daisy dots / x's
    91: (71, (25, 49), (49, 97)),                                   # shovel
}

# text labels the original prints into the picture (object type 01)
LABELS = {
    67: ("DOORWAY", 0, 0),
    68: ("WELCOME", 12, 8),         # centred in the porch mat of picture 1
    69: ("NOTE", 3, 2),
    72: ("MATCHES", 2, 2),
}

NUM_PICTURES = 95


def compose(apics):
    """Pico-8 picture number -> (islands in local coordinates, label)."""
    out = {}
    for pic in range(1, NUM_PICTURES + 1):
        islands, label = [], None
        if pic in ROOMS or pic in BACKYARD:
            parts = ROOMS[pic] if pic in ROOMS else [BACKYARD[pic]]
            for part in parts:
                ap, dx, dy = (part, 0, 0) if isinstance(part, int) else part
                islands += [[(x + dx, y + dy) for x, y in l] for l in apics[ap - 1]]
        elif pic in OBJECTS:
            ap, (px, py), rec = OBJECTS[pic]
            dx, dy = (0, 0) if rec is None else (rec[0] - 1 - 2 * px, rec[1] - 1 - 2 * py)
            dx, dy = max(dx, -1), max(dy, -1)   # Apple object pictures start at (1, 1)
            islands = [[(x + dx, y + dy) for x, y in l] for l in apics[ap - 1]]
        elif pic in LABELS:
            label = LABELS[pic]
        else:
            raise SystemExit("no Apple picture for Pico-8 picture %d" % pic)
        out[pic] = (islands, label)
    return out

# -- delta encoding -----------------------------------------------------------
#
#   b < 0xF0        line by (b >> 4) - 7, (b & 15) - 7     (-7..7 each)
#   F0 dx dy        line by signed bytes
#   F1 x y          line to absolute local point
#   F2 dx           horizontal line by signed byte
#   F3 dy           vertical line by signed byte
#   FD x y text 00  text label at local point
#   FE x y          move to local point (new polyline)
#   FF              end of picture
#
# Local coordinates are added to the drawing origin (twice the Pico-8 object
# position; 0 for room pictures).

SHORT_MAX = 0xF0
OP_LONG, OP_ABS, OP_HORZ, OP_VERT = 0xF0, 0xF1, 0xF2, 0xF3
OP_TEXT, OP_MOVE, OP_END = 0xFD, 0xFE, 0xFF


def encode(islands, label):
    out = bytearray()
    if label:
        text, lx, ly = label
        out += bytes([OP_TEXT, lx, ly]) + text.encode("ascii") + b"\0"
    for line in islands:
        x, y = line[0]
        assert 0 <= x < 256 and 0 <= y < 256, (x, y)
        out += bytes([OP_MOVE, x, y])
        if len(line) == 1:
            out += bytes([0x77])                # zero-length line: a dot
        for nx, ny in line[1:]:
            dx, dy = nx - x, ny - y
            if -7 <= dx <= 7 and -7 <= dy <= 7:
                out.append(((dx + 7) << 4) | (dy + 7))
            elif dy == 0 and -128 <= dx <= 127:
                out += bytes([OP_HORZ, dx & 0xFF])
            elif dx == 0 and -128 <= dy <= 127:
                out += bytes([OP_VERT, dy & 0xFF])
            elif -128 <= dx <= 127 and -128 <= dy <= 127:
                out += bytes([OP_LONG, dx & 0xFF, dy & 0xFF])
            else:
                out += bytes([OP_ABS, nx, ny])
            x, y = nx, ny
    out.append(OP_END)
    return bytes(out)


def decode(data):
    """Reference decoder (tests): list of polylines, label."""
    lines, label, p = [], None, 0
    while data[p] != OP_END:
        op = data[p]
        if op == OP_TEXT:
            end = data.index(0, p + 3)
            label = (data[p + 3:end].decode(), data[p + 1], data[p + 2])
            p = end + 1
        elif op == OP_MOVE:
            lines.append([(data[p + 1], data[p + 2])])
            p += 3
        else:
            x, y = lines[-1][-1]
            if op == OP_LONG:
                dx, dy = [v - 256 if v > 127 else v for v in data[p + 1:p + 3]]
                p += 3
            elif op in (OP_HORZ, OP_VERT):
                v = data[p + 1] - 256 if data[p + 1] > 127 else data[p + 1]
                dx, dy = (v, 0) if op == OP_HORZ else (0, v)
                p += 2
            elif op == OP_ABS:
                dx, dy = data[p + 1] - x, data[p + 2] - y
                p += 3
            else:
                dx, dy = (op >> 4) - 7, (op & 15) - 7
                p += 1
            lines[-1].append((x + dx, y + dy))
    return lines, label

# -- title screen -------------------------------------------------------------

TITLE_GREEN = 0x3       # TMS9918 light green
TITLE_MAGENTA = 0xD
TITLE_WHITE = 0xF
TITLE_BLACK = 0x1


def title_screen(files):
    """Apple hi-res page 1 -> Graphics II pattern + colour tables (6 KB each).
    The picture is 245 px wide, so the left 256 of 280 columns are taken 1:1.
    A pixel without lit neighbours is a colour pixel on the Apple; an 8-pixel
    cell takes the colour of its colour pixels, otherwise white."""
    obj = files["AUTO LOAD OBJ"]
    hires = obj[0x2000 - 0x0FFD:0x4000 - 0x0FFD]
    pattern, colour = bytearray(6144), bytearray(6144)
    for y in range(192):
        a = (y % 8) * 0x400 + ((y // 8) % 8) * 0x80 + (y // 64) * 0x28
        bits, pal = [], []
        for b in range(40):
            v = hires[a + b]
            for i in range(7):
                bits.append((v >> i) & 1)
                pal.append(v >> 7)
        for cx in range(32):
            byte, greens, magentas, whites = 0, 0, 0, 0
            for i in range(8):
                x = cx * 8 + i
                if not bits[x]:
                    continue
                byte |= 0x80 >> i
                lonely = not (x > 0 and bits[x - 1]) and not (x < 279 and bits[x + 1])
                if not lonely:
                    whites += 1
                elif pal[x] == 0 and (x & 1):
                    greens += 1
                elif pal[x] == 0:
                    magentas += 1
                else:
                    whites += 1
            ink = TITLE_WHITE
            if greens > whites and greens >= magentas:
                ink = TITLE_GREEN
            elif magentas > whites:
                ink = TITLE_MAGENTA
            off = (y // 8) * 256 + cx * 8 + y % 8
            pattern[off] = byte
            colour[off] = (ink << 4) | TITLE_BLACK
    return bytes(pattern), bytes(colour)


# -- instructions ------------------------------------------------------------

def instruction_pages(files):
    """The instruction pages printed by AUTO LOAD OBJ, lower case, one string
    per page.  Each piece of text ends with 0 and is followed by JSRs (print,
    wait for RETURN, clear the screen); the lines are already wrapped at 40
    columns, the screen width of the port too."""
    data = files["AUTO LOAD OBJ"]
    plain = bytes(b & 0x7F for b in data)
    i = plain.index(b"INSTRUCTIONS FOR HI-RES")
    end = plain.index(b"PRESS RETURN TO BEGIN PLAY") + len("PRESS RETURN TO BEGIN PLAY")
    text = []
    while i < end:
        b = data[i]
        if b == 0:
            i += 1
            while data[i] == 0x20:          # JSR
                i += 3
            continue
        assert b == 0x8D or b >= 0xA0, "unexpected byte %02X" % b
        text.append("\n" if b == 0x8D else chr(b & 0x7F))
        i += 1
    pages = "".join(text).lower().split("press return to continue")
    return [p + "press return to continue" for p in pages[:-1]] + [pages[-1]]


def build(root):
    files = read_disk(os.path.join(root, DISK))
    apics = apple_pictures(files)
    assert len(apics) == 97, len(apics)
    pictures = {}
    for pic, (islands, label) in compose(apics).items():
        data = encode(islands, label)
        expect = [list(l) if len(l) > 1 else [l[0], l[0]] for l in islands]
        assert decode(data) == (expect, label), pic
        pictures[pic] = data
    pattern, colour = title_screen(files)
    title = (bytes(zx7.compress(pattern)), bytes(zx7.compress(colour)))
    return pictures, title
