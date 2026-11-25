# TODO - Mystery House for the SC-3000

State: completely playable. 48 KB ROM without mapper (also runs on the SD-1000
flash cart, whose firmware loads the first 48 KB 1:1). Pictures, title screen
and texts come from the Apple II original (apple2/). The game logic is checked
on the PC (`make test`), the walkthrough was played in Emulicious with the
SC-3000 keyboard. Real SD-1000 hardware is **not** tested.

## Open / ideas

- `save game` / `restore game` keep the game in RAM only, until the console is
  switched off; the SD-1000 has no battery backed SRAM.
- Pictures are drawn pixel by pixel with VRAM read-modify-write (a visible
  build-up like on the Apple II). If needed, `gfx_line()` in assembler.
- ROM: ~8.6 KB free (room rules as tables, trimmed SGlib/PSGlib, title music
  1.2 KB).
- The picture labels (DOORWAY, NOTE, MATCHES, WELCOME) use the port's 5x7
  font, not the Apple II one.
- The temp room numbers (1..10: cabinet, refrigerator, ...) are still plain
  numbers in rooms.c.
