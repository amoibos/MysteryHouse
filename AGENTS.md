# AGENTS.md

This file provides guidance to coding agents (Codex, Claude Code, ...) when working with code in this repository.

## Project Overview

Port of **Mystery House** (1980, Ken & Roberta Williams) - the first graphic adventure game - to the **Sega SC-3000** (keyboard required, 2 KB RAM). 48 KB cartridge without mapper (runs on the **SD-1000** flash cart and any SC-3000 cartridge). Game logic follows the Pico-8 remake by Christopher Drum in `original/` (read-only reference); pictures and the title screen come from the Apple II original in `apple2/` (DOS 3.3 disk image, read-only). The port is C for the Z80, built with SDCC and devkitSMS (SGlib, crt0_sg, ihx2sms).

## Building

```
make            # mystery_house.SC, 48 KB (objects in build/)
make test       # game logic on the PC (gcc): walkthrough with plain and BPE packed messages
make data       # regenerate gamedata.c/.h + assets/assets.c + assets/texts.c from original/ + apple2/ (Python only)
make clean
```

Emulator check (Emulicious with DAP on port 58870 running):
```
python tools/emulicious_run.py --rom <copy>.sc --commands tests/walkthrough.txt --expect "good-bye"
```
Load a *copy* of the ROM from a folder without `.map/.noi/.c` files, otherwise Emulicious switches its expression evaluator to C mode and `pc`/`@$addr`/`vram@@` stop working.

## ROM / RAM layout

Everything is linked flat into 0x0000-0xBFFF (~39.4 KB of 48 KB used, ~8.6 KB free): code, SGlib (trimmed to the used functions), PSGlib (music only), tables (`gamedata.c`), BPE packed texts (messages, descriptions, intro pages in `assets/texts.c`), 6x8 font (`assets/font6.c`) and the asset block `assets/assets.c` (95 Apple II pictures delta coded, title screen ZX7), title music `assets/music.c` (Saint-Saens: Danse macabre, 41 s, 3 voices without noise drums, PSGlib compressed 1.2 KB, played once on the title screen only; `tools/make_music.py`, usage line in its docstring). No mapper.

RAM: ~0.63 KB of variables from 0xC000 (largest: BPE expand buffer 160 B, game state 124 B + RAM save copy 124 B), stack at 0xFFF0 (2 KB SC-3000 RAM is mirrored). The transcript history (23 lines x 40 bytes) lives in VRAM at 0x3800, the sprite pattern table that is unused because no sprites are shown - like the Apple II, whose text page keeps the lines scrolled out of the 4-line window. Fits the native SC-3000; does **not** fit the 1 KB SG-1000.

## C Source File Roles

| File | Purpose |
|---|---|
| `main.c` | boot: title, public-domain page, g/i prompt, instructions, game loop |
| `gamedata.h/c` | **generated** by `tools/mh_convert.py`: `V_*`/`N_*`/`OBJ_*`/`PIC_*`/`SPR_*`/`ROOM_*` enums (room names in `ROOM_NAMES`), sorted vocabulary, object defs, drop positions, room descriptions/start pictures, temp rooms, asset index |
| `tools/apple2.py` | reads the Apple II disk: pictures (BLOCK1-19), object table (STARTUP), title (hi-res page in AUTO LOAD OBJ), instruction pages (AUTO LOAD OBJ); `ROOMS`/`OBJECTS`/`LABELS` map every Pico-8 picture number to Apple pictures; delta encoding |
| `bpe.h/c`, `tools/pack_texts.py` | build step: string literals of rooms/general/objects, room/object descriptions and the intro texts are byte pair encoded into `build/src`; `transcribe()` expands with `bpe_text()`, `text_page()` streams with `bpe_getc()` |
| `tools/make_music.py`, `tools/vgm_fix.py` | title music: MIDI -> 3 PSG voices -> midi2vgm -> PSGlib data (needs midi2vgm and vgm2psg/psgcomp, see the docstring) |
| `tools/zx7.py` | ZX7 compressor for the title screen (SGlib decompresses straight into VRAM) |
| `state.h/c` | `sGame` (flags, timers, rooms, `obj_loc[]`, room pictures, 5 mutable objects), `game_init()` |
| `objects.h/c` | mh.p8 object helpers: `key_in_room`, `noun_in_room`, `decorate`, `swap_*`, `look_inside`, save/restore (RAM) |
| `rules.h/c` | `ON(v,n)` / `UNHANDLED()` = Lua `room[verb][noun]` lookup semantics; `sRule` tables (`MSG`/`GO`) for the plain string/number entries, `run_rules()` |
| `rooms.h/c` | rooms.lua: per room a rule table plus a function for the remaining entries, dining room `before`, entry hall `after` |
| `general.h/c` | general.lua: room independent verbs |
| `parser.h/c` | `parse_input()`, universal get/drop/look/go fallbacks, `run_timers()`, room description |
| `vocabulary.h/c` | binary search in the generated word tables, direction verbs |
| `views.h/c` | `view_update()` redraws backdrop/decorations/room inventory only when something changed |
| `pictures.h/c` | picture stream decoder (`draw_image`: lines + text labels at 2x the Pico-8 object position), `draw_title()` |
| `gfx.h/c` | Graphics II bitmap: clear rows, Bresenham line, label text, `gfx_clip_bottom` |
| `textio.h/c` | 40-column text, transcript ring buffer in VRAM, word wrap, `-more-` paging, keyboard matrix + edge detection, input line, full-screen text pages |
| `tests/` | host harness (`host_io.c` replaces textio/views), `walkthrough.txt`, `run_tests.sh` |

## Architecture

```
main -> begin_game() -> loop game_turn():
  show_room()      current_room = next_room; view_update(); description on room/temp change
  text_input()     row 23, "> "
  parse_input()    split like Lua (8-char words, bare direction -> go)
     room_before() -> room_dispatch() -> general_dispatch() -> universal_fallback() -> room_after()
  run_timers()     match, "getting dark" (turn 20), darkness (turn 36), win check (next_room == ROOM_FRONT_YARD)
```

Object locations (`obj_loc[]`): `0` none, `1..41` room decoration (drawn at its own position), `LOC_DROPPED|room` room inventory (drawn at drop positions in object order), `LOC_TEMP|t` temp room decoration, `LOC_INV`. This replaces the Lua key sets; `key_in_room` / `noun_in_room` keep the original temp-room rules (a temp room hides the room decorations).

Screen: picture 256x160 (Apple II coordinates 1:1, rows 0-19), transcript rows 20-22, input row 23. Empty input toggles a full-screen transcript (Pico-8 "toggle image"). Text is never lost: when more lines than visible arrive, `-more-` waits; a letter typed there starts the next command.

## Key Gotchas

- **Texts follow the Apple II original**: instructions, public domain page (`PUBLIC_DOMAIN` in mh_convert.py, without the 1987 address) and messages use its wording; only "your apple computer" became "your sc-3000".
- **Quirks of the original are kept on purpose** (comments `BUG`), e.g. dagger does not swap Joe's dots, shovel teleport. Unreachable Lua tables (room 15 `climb`, room 23 `up`, room 18's first `look`) are omitted.
- **crt0 and SGlib are built from source** by the Makefile. The old prebuilt `crt0_sg.rel` put the stack at 0xC3F0 (SG-1000) right inside the game variables. `SGlib/SGlib.c` is trimmed to what the game calls (joypad/pause variables commented out); PSGlib (`psg/`) is built from source too, SFX code and variables under `#if 0` (`#` immediates patched, the prebuilt lib uses the old calling convention).
- **Picture numbers stay the Pico-8 ones** (game logic sets them); Pico-8 split some scenes differently: the back yard is one Apple picture per door/gate state (`PIC_BACKYARD_FIRST`, views.c), the dead bodies are separate Apple pictures composed into the room pictures, DOORWAY/NOTE/MATCHES/WELCOME are text labels (the Apple object table type 01).
- Only literals that go to `transcribe()` may be packed: `parser.c` (prompts, `append()`) stays plain; in `gamedata.c` only `room_descr`/`obj_descr`.
- **SDCC outputs `.rel` to CWD** unless `-o` is given; the Makefile always uses `-o build/...`.
- Room numbers are the rooms.lua ones; the code uses the generated `ROOM_*` names, the handler functions keep the numbers (`r1`..`r41`).
- `make data` takes ~20 s: the ZX7 optimizer for the title screen is slow in Python.
- Text: 40 columns of the 5x7 font in `assets/font6.c` (generated by `tools/make_font6.py`), 8 px margin left/right; `put_row()` shifts the glyphs into tile patterns. The Pico-8 texts stay lower case.
