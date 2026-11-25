#!/usr/bin/env python3
"""Mystery House: Pico-8 sources -> C data for the SC-3000 port.

Reads the read-only reference implementation in original/ and writes

    gamedata.h / gamedata.c     tables (vocabulary, objects, rooms, picture index)
    assets/assets.c             pictures and title screen from the Apple II
                                original (tools/apple2.py)
    assets/texts.c              public domain page and instructions in the
                                wording of the Apple II original

usage: python tools/mh_convert.py
"""

import argparse
import os
import re

import apple2

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIG = os.path.join(ROOT, "original")



def read(name):
    with open(os.path.join(ORIG, name), encoding="utf-8") as f:
        return f.read()


# -- public domain page ------------------------------------------------------

# MYSTERY.HELLO of the Apple II disk; the BASIC program pads its lines to 40
# columns, here the words are rejoined.  The 1987 postal address is left out.
PUBLIC_DOMAIN = (
    "\tfrom a great moment in history: mystery house, the first graphic "
    "adventure, was created in 1980 by sierra on-line founders ken and roberta "
    "williams, and released into the public domain in 1987 to celebrate "
    "sierra's 7th anniversary.\n"
    "\twe encourage you to copy this game, for yourself or to share. if you "
    "have enjoyed it you may send a $5 donation to ken and roberta's favorite "
    "charity, sierra historic sites association.\n"
    "\tnote: because it is now free, sierra's gift to you, we can no longer "
    "take calls on this game. if you need help, we suggest you contact your "
    "local apple users group to talk with others who may have played it.\n"
    "\tsc-3000 port: game logic from the remake by christopher drum, pictures "
    "and title screen from the apple ii original.")


# -- rooms -------------------------------------------------------------------

ROOM_NAMES = [
    "FRONT_YARD", "PORCH", "ENTRY_HALL", "KITCHEN",
    "FOREST_1", "FOREST_2", "FOREST_3", "FOREST_4",             # 5-12: the maze
    "FOREST_5", "FOREST_6", "FOREST_7", "FOREST_8",
    "PINE_TREE", "LIBRARY", "SIDE_YARD", "DINING_ROOM", "BACK_YARD", "CEMETERY",
    "PANTRY", "PASSAGEWAY", "BASEMENT", "TUNNEL", "TREETOP", "JUNCTION",
    "DOORWAY_LARGE_BEDROOM", "LARGE_BEDROOM", "DOORWAY_SMALL_BEDROOM", "SMALL_BEDROOM",
    "DOORWAY_NURSERY", "NURSERY", "DOORWAY_BOYS_BEDROOM", "BOYS_BEDROOM",
    "STAIRWAY", "DOORWAY_STUDY", "STUDY", "CRAWLSPACE", "ON_STAIRWAY", "BATHROOM",
    "ATTIC", "STORAGE_ROOM", "TOWER",
]


# -- Lua parsing helpers -----------------------------------------------------

def lua_string_list(src, name):
    body = re.search(name + r"\s*=\s*\{(.*?)\n\}", src, re.S).group(1)
    return re.findall(r'"([^"]*)"', body)


def c_ident(s):
    s = s.replace("'", "").replace(".", "")
    return re.sub(r"[^A-Za-z0-9]", "_", s).upper()


def c_str(s):
    out = []
    for ch in s:
        if ch == '"' or ch == "\\":
            out.append("\\" + ch)
        elif ch == "\n":
            out.append("\\n")
        elif 32 <= ord(ch) < 127:
            out.append(ch)
        else:
            out.append("\\%03o" % ord(ch))     # octal has a fixed length, \x does not
    return '"' + "".join(out) + '"'


def c_bytes(data, indent="    ", per_line=16):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append(indent + ",".join("0x%02x" % b for b in data[i:i + per_line]) + ",")
    return "\n".join(lines)


# -- Vocabulary --------------------------------------------------------------

def build_vocab(raw):
    """Mimic init_vocabulary(): every synonym (cut to 8 chars) maps to the
    primary word; later entries overwrite earlier ones (push -> press)."""
    primaries = []
    table = {}
    for line in raw:
        words = line.split(",")
        common = words[0][:8]
        primaries.append(common)
        for w in words:
            table[w[:8]] = common
    return primaries, table


# -- Objects -----------------------------------------------------------------

def parse_objects(var_pics):
    defs = max(re.findall(r'object_defs = "([^"]*)"', read("objects.lua")), key=len)
    over = re.search(r'overflow = "(.*?)"', read("objects_overflow.lua")).group(1)
    objs = []
    for d in (defs + over).split("|"):
        if not d:
            continue
        f = d.split(",")
        name, dict_name, descr, room, pic, x, y = f[:7]
        temp = int(f[7]) if len(f) > 7 else 0
        objs.append(dict(name=name, noun=dict_name,
                         descr=None if descr == "nil" else descr,
                         room=int(room), pic=var_pics[pic],
                         x=int(x), y=int(y), temp=temp))
    return objs


def parse_vars():
    src = read("vars.lua")
    pics = {m.group(1): int(m.group(2))
            for m in re.finditer(r"^((?:pic|spr)_\w+)\s*=\s*(\d+)", src, re.M)}
    sprites = [tuple(int(v) for v in m.group(1).split(","))
               for m in re.finditer(r"\{(\d+,\d+,\d+,\d+,\d+)\}", src)]
    return pics, sprites


# -- Rooms -------------------------------------------------------------------

def parse_rooms(var_pics):
    src = read("rooms.lua")
    start = re.search(r"room_start_pictures = \{(.*?)\}", src, re.S).group(1)
    start = [var_pics[w] for w in re.findall(r"(pic_\w+)", start)]
    descr = re.findall(r'^\t\tdescription = "([^"]*)"', src, re.M)
    temps_src = re.search(r"temps = \{(.*?)\n\}", src, re.S).group(1)
    temps = []
    for blk in re.findall(r"\{\s*(?:--\d+)?\s*picture = (pic_\w+)(.*?)\n\t\}", temps_src, re.S):
        deco = re.findall(r"(\w+) = 1", blk[1])
        temps.append((var_pics[blk[0]], deco))
    assert len(start) == len(descr) == len(ROOM_NAMES) and len(temps) == 10
    return start, descr, temps


# -- main --------------------------------------------------------------------

def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    var_pics, _ = parse_vars()

    vsrc = read("vocabulary.lua")
    verbs, verb_table = build_vocab(lua_string_list(vsrc, "raw_verbs"))
    nouns, noun_table = build_vocab(lua_string_list(vsrc, "raw_nouns"))
    objs = parse_objects(var_pics)
    start_pics, descriptions, temps = parse_rooms(var_pics)
    obj_index = {o["name"]: i for i, o in enumerate(objs)}

    # -- intro texts: the Apple II wording --
    instructions = apple2.instruction_pages(apple2.read_disk(os.path.join(ROOT, apple2.DISK)))
    instructions = [re.sub(r"if your copy should ever fail to load.*?93614\n", "",
                           p.replace("your apple computer", "your sc-3000"), flags=re.S)
                    for p in instructions]
    assert all(p.count("\n") < 23 for p in instructions), "page longer than the screen"
    # pictures and title screen come from the Apple II original (apple2.py);
    # the Pico-8 pictures only define the numbering (PIC_* / SPR_*)
    apple_pics, title = apple2.build(ROOT)

    # -- assets --
    items = [("pic%d" % i, apple_pics[i]) for i in range(1, apple2.NUM_PICTURES + 1)]
    items += [("title_patterns", title[0]), ("title_colours", title[1])]
    blob, where = bytearray(), {}
    for key, data in items:
        where[key] = len(blob)
        blob += data

    os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)
    with open(os.path.join(ROOT, "assets", "assets.c"), "w", newline="\n") as f:
        f.write("/* generated by tools/mh_convert.py - do not edit */\n\n")
        f.write("const unsigned char asset_data[%d] = {\n" % len(blob))
        f.write(c_bytes(blob) + "\n};\n")

    # intro texts as string literals: the build packs them (tools/pack_texts.py)
    with open(os.path.join(ROOT, "assets", "texts.c"), "w", newline="\n") as f:
        f.write("/* generated by tools/mh_convert.py - do not edit */\n")
        f.write('#include "gamedata.h"\n\n')
        f.write("const char public_domain_text[] =\n    %s;\n\n" % c_str(PUBLIC_DOMAIN))
        f.write("const char * const instruction_text[NUM_INSTRUCTION_PAGES] = {\n")
        for t in instructions:
            f.write("    %s,\n" % c_str(t))
        f.write("};\n")

    # -- gamedata.h --
    h = []
    h.append("/* generated by tools/mh_convert.py from the Lua sources in original/ and the Apple II disk in apple2/ - do not edit */")
    h.append("#ifndef GAMEDATA_H\n#define GAMEDATA_H\n")
    h.append("/* verbs: primary words of raw_verbs, V_NONE = unknown word */")
    h.append("enum {\n    V_NONE = 0,")
    for v in verbs:
        h.append("    V_%s," % c_ident(v))
    h.append("    NUM_VERBS\n};\n")
    h.append("/* nouns: primary words of raw_nouns; N_EMPTY is the \"\" entry (no noun) */")
    h.append("enum {\n    N_NONE = 0,")
    for n in nouns:
        h.append("    N_%s," % (c_ident(n) if n else "EMPTY"))
    h.append("    NUM_NOUNS\n};\n")
    h.append("/* objects in object_order (inventory listing / drawing order) */")
    h.append("enum {")
    for i, o in enumerate(objs):
        h.append("    OBJ_%s%s," % (c_ident(o["name"]), " = 0" if i == 0 else ""))
    h.append("    NUM_OBJECTS\n};\n")
    for name, num in sorted(var_pics.items(), key=lambda kv: kv[1]):
        h.append("#define %-34s %d" % (c_ident(name), num))
    h.append("#define PIC_BACKYARD_FIRST                 %d  /* + 2 * door open + gate open */"
             % apple2.BACKYARD_FIRST)
    h.append("#define NUM_PICTURES                       %d\n" % apple2.NUM_PICTURES)
    h.append("/* rooms.lua numbers its rooms; the names follow the descriptions */")
    h.append("enum {\n    ROOM_NONE,")
    for name in ROOM_NAMES:
        h.append("    ROOM_%s," % name)
    h.append("};")
    h.append("#define NUM_ROOMS       %d" % len(ROOM_NAMES))
    h.append("#define NUM_TEMPS       %d" % len(temps))
    h.append("#define NUM_DROP_POS    21")
    h.append("#define NUM_INSTRUCTION_PAGES %d\n" % len(instructions))
    h.append("typedef struct {\n    unsigned char noun;      /* N_* dictionary word */\n"
             "    unsigned char room;      /* initial room (0 = nowhere) */\n"
             "    unsigned char pic;       /* PIC_* / SPR_* */\n"
             "    unsigned char x, y;      /* draw position (Pico-8 pixels) */\n"
             "    unsigned char temp;      /* temp room opened by 'read' (notes) */\n"
             "} sObjectDef;\n")
    h.append("/* sorted words for the binary search in vocabulary.c, with the id of their primary word */")
    h.append("#define NUM_VERB_WORDS  %d" % len(verb_table))
    h.append("#define NUM_NOUN_WORDS  %d" % len(noun_table))
    h.append("extern const char * const verb_words[NUM_VERB_WORDS];")
    h.append("extern const unsigned char verb_ids[NUM_VERB_WORDS];")
    h.append("extern const char * const noun_words[NUM_NOUN_WORDS];")
    h.append("extern const unsigned char noun_ids[NUM_NOUN_WORDS];")
    h.append("extern const sObjectDef obj_def[NUM_OBJECTS];")
    h.append("extern const char * const obj_descr[NUM_OBJECTS];")
    h.append("extern const unsigned char drop_pos[NUM_DROP_POS][2];")
    h.append("extern const unsigned char room_start_pic[NUM_ROOMS + 1];")
    h.append("extern const char * const room_descr[NUM_ROOMS + 1];")
    h.append("extern const unsigned char temp_pic[NUM_TEMPS + 1];")
    h.append("extern const unsigned char temp_initial_obj[NUM_TEMPS + 1];  /* 0xFF = none */")
    h.append("extern const unsigned char * const picture_data[NUM_PICTURES + 1];  /* see tools/apple2.py */")
    h.append("extern const unsigned char * const title_patterns;   /* ZX7, Graphics II tables */")
    h.append("extern const unsigned char * const title_colours;")
    h.append("extern const char public_domain_text[];                      /* assets/texts.c */")
    h.append("extern const char * const instruction_text[NUM_INSTRUCTION_PAGES];\n")
    h.append("#endif")

    # -- gamedata.c --
    c = []
    c.append("/* generated by tools/mh_convert.py from the Lua sources in original/ and the Apple II disk in apple2/ - do not edit */")
    c.append('#include "gamedata.h"\n')
    c.append("extern const unsigned char asset_data[];\n")

    def vocab_tables(prefix, table, primaries, enum_prefix):
        words = sorted(table.items())
        c.append("const char * const %s_words[NUM_%s_WORDS] = {" % (prefix, prefix.upper()))
        for w, _ in words:
            c.append("    %s," % c_str(w))
        c.append("};")
        c.append("const unsigned char %s_ids[NUM_%s_WORDS] = {" % (prefix, prefix.upper()))
        for _, p in words:
            ident = c_ident(p) if p else "EMPTY"
            c.append("    %s_%s," % (enum_prefix, ident))
        c.append("};\n")

    vocab_tables("verb", verb_table, verbs, "V")
    vocab_tables("noun", noun_table, nouns, "N")

    c.append("const sObjectDef obj_def[NUM_OBJECTS] = {")
    for o in objs:
        assert noun_table.get(o["noun"]) == o["noun"], o["noun"]
        noun = "N_" + c_ident(o["noun"])
        c.append("    {%-14s %2d, %3d, %3d, %3d, %d},  /* %s */" %
                 (noun + ",", o["room"], o["pic"], o["x"], o["y"], o["temp"], o["name"]))
    c.append("};\n")
    c.append("const char * const obj_descr[NUM_OBJECTS] = {")
    for o in objs:
        c.append("    %s," % (c_str(o["descr"]) if o["descr"] else "0"))
    c.append("};\n")

    drops = re.search(r"drop_positions = \{(.*?)\}\n", read("objects.lua")).group(1)
    drops = re.findall(r"\{(\d+),(\d+)\}", drops)
    assert len(drops) == 21
    c.append("const unsigned char drop_pos[NUM_DROP_POS][2] = {")
    c.append("    " + ",".join("{%s,%s}" % d for d in drops))
    c.append("};\n")

    c.append("const unsigned char room_start_pic[NUM_ROOMS + 1] = {\n    0,")
    c.append("    " + ",".join(str(p) for p in start_pics))
    c.append("};\n")
    c.append("const char * const room_descr[NUM_ROOMS + 1] = {\n    0,")
    for d in descriptions:
        c.append("    %s," % c_str(d))
    c.append("};\n")

    c.append("const unsigned char temp_pic[NUM_TEMPS + 1] = {\n    0,")
    c.append("    " + ",".join(str(t[0]) for t in temps))
    c.append("};\n")
    c.append("const unsigned char temp_initial_obj[NUM_TEMPS + 1] = {\n    0xFF,")
    ids = []
    for _, deco in temps:
        assert len(deco) <= 1
        ids.append("OBJ_" + c_ident(deco[0]) if deco else "0xFF")
    c.append("    " + ",".join(ids))
    c.append("};\n")

    def asset(key):
        return "asset_data + %d" % where[key]

    c.append("const unsigned char * const picture_data[NUM_PICTURES + 1] = {\n    0,")
    for i in range(1, apple2.NUM_PICTURES + 1):
        c.append("    %s," % asset("pic%d" % i))
    c.append("};\n")
    c.append("const unsigned char * const title_patterns = %s;" % asset("title_patterns"))
    c.append("const unsigned char * const title_colours = %s;" % asset("title_colours"))

    with open(os.path.join(ROOT, "gamedata.h"), "w", newline="\n") as f:
        f.write("\n".join(h) + "\n")
    with open(os.path.join(ROOT, "gamedata.c"), "w", newline="\n") as f:
        f.write("\n".join(c) + "\n")

    print("assets: %d bytes" % len(blob))
    print("objects %d, verbs %d (%d words), nouns %d (%d words)" %
          (len(objs), len(verbs), len(verb_table), len(nouns), len(noun_table)))


if __name__ == "__main__":
    main()
