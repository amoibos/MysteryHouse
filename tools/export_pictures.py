#!/usr/bin/env python3
"""Export the pictures of the port as PNG files.

Every picture (PIC_* / SPR_* number, including the back yard variants
92-95) is decoded from the delta stream that tools/apple2.py writes and
drawn like the SC-3000 does (Bresenham lines, labels in the 5x7 font of
assets/font6.c), white on black at its own origin; the title screen is
rendered from its pattern and colour tables.

usage: python tools/export_pictures.py [--out export] [--scale 2]
"""

import argparse
import os
import re

from PIL import Image

import apple2

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE_H = 160           # the picture area, as gfx.h

# TMS9918 colours 0-15 (0 = transparent, shown as black)
TMS_PALETTE = [
    (0, 0, 0), (0, 0, 0), (33, 200, 66), (94, 220, 120), (84, 85, 237), (125, 118, 252),
    (212, 82, 77), (66, 235, 245), (252, 85, 84), (255, 121, 120), (212, 193, 84),
    (230, 206, 128), (33, 176, 59), (201, 91, 186), (204, 204, 204), (255, 255, 255),
]


def picture_names():
    """picture number -> name from gamedata.h (first name, PIC_NUM is an alias)"""
    names = {}
    with open(os.path.join(ROOT, "gamedata.h"), encoding="utf-8") as f:
        for name, num in re.findall(r"#define ((?:PIC|SPR)_\w+)\s+(\d+)", f.read()):
            if name not in ("PIC_NUM", "PIC_BACKYARD_FIRST"):
                names.setdefault(int(num), name.lower())
    first = apple2.BACKYARD_FIRST
    for i in range(4):
        names[first + i] = "backyard_door_%s_gate_%s" % (("closed", "open")[i >> 1],
                                                        ("closed", "open")[i & 1])
    return names


def load_font():
    with open(os.path.join(ROOT, "assets", "font6.c"), encoding="utf-8") as f:
        src = f.read()
    return [int(v, 16) for v in re.findall(r"0x([0-9a-f]{2})", src.split("{", 1)[1])]


def line(px, x0, y0, x1, y1):
    """gfx_line(): Bresenham, endpoints included"""
    dx, dy = abs(x1 - x0), abs(y1 - y0)
    sx, sy = (1 if x1 > x0 else -1), (1 if y1 > y0 else -1)
    err = dx - dy
    while True:
        if 0 <= x0 < 256 and y0 < IMAGE_H:
            px[x0, y0] = 1
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x0 += sx
        if e2 < dx:
            err += dx
            y0 += sy


def text(px, font, x, y, s):
    """gfx_text(): 6 pixel cells"""
    for ch in s:
        if " " < ch <= "~":
            glyph = font[(ord(ch) - 32) * 8:(ord(ch) - 32) * 8 + 7]
            for r, bits in enumerate(glyph):
                for c in range(8):
                    if bits & (0x80 >> c) and x + c < 256 and y + r < IMAGE_H:
                        px[x + c, y + r] = 1
        x += 6


def render_picture(data, font):
    img = Image.new("1", (256, IMAGE_H), 0)
    px = img.load()
    polylines, label = apple2.decode(data)
    clamp = lambda v: min(v, 255)
    for pl in polylines:
        if len(pl) == 1:
            pl = pl * 2
        for (x0, y0), (x1, y1) in zip(pl, pl[1:]):
            line(px, clamp(x0), clamp(y0), clamp(x1), clamp(y1))
    if label:
        text(px, font, label[1], label[2], label[0])
    return img


def render_title(pattern, colour):
    img = Image.new("RGB", (256, 192))
    px = img.load()
    for y in range(192):
        for x in range(256):
            i = (y // 8) * 256 + (x // 8) * 8 + y % 8
            on = pattern[i] & (0x80 >> (x % 8))
            px[x, y] = TMS_PALETTE[(colour[i] >> 4) if on else (colour[i] & 15)]
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=os.path.join(ROOT, "export"))
    ap.add_argument("--scale", type=int, default=2)
    args = ap.parse_args()

    files = apple2.read_disk(os.path.join(ROOT, apple2.DISK))
    names = picture_names()
    font = load_font()
    pictures = {pic: apple2.encode(islands, label)
                for pic, (islands, label) in apple2.compose(apple2.apple_pictures(files)).items()}

    os.makedirs(args.out, exist_ok=True)
    size = lambda img: img.resize((img.width * args.scale, img.height * args.scale), Image.NEAREST)
    for pic in sorted(pictures):
        name = "%02d_%s.png" % (pic, names.get(pic, "picture"))
        size(render_picture(pictures[pic], font).convert("RGB")).save(os.path.join(args.out, name))
    size(render_title(*apple2.title_screen(files))).save(os.path.join(args.out, "00_title.png"))
    print("%d pictures + title -> %s" % (len(pictures), args.out))


if __name__ == "__main__":
    main()
