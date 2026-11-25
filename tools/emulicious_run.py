#!/usr/bin/env python3
"""Drive mystery_house.SC in Emulicious over its DAP debug port.

Emulicious must be running with remote debugging enabled (default port
58870).  The script loads the ROM, types commands with the SC-3000 keyboard
(DAP keyboardEvent), and after every command

  * reconstructs the 256x192 bitmap from the Graphics II pattern table and
    saves it as PNG (only for commands listed with --shots, and the last one)
  * reads the text rows back by matching the patterns against the font

usage:
  python tools/emulicious_run.py --commands tests/walkthrough.txt --expect "guru wizard"
"""

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# TMS9918 colours 0-15 (0 = transparent, shown as black)
TMS_PALETTE = [
    (0, 0, 0), (0, 0, 0), (33, 200, 66), (94, 220, 120), (84, 85, 237), (125, 118, 252),
    (212, 82, 77), (66, 235, 245), (252, 85, 84), (255, 121, 120), (212, 193, 84),
    (230, 206, 128), (33, 176, 59), (201, 91, 186), (204, 204, 204), (255, 255, 255),
]

# Java virtual key codes used by Emulicious' keyboardEvent
JAVA_KEYS = {" ": 32, "\n": 10, "\b": 8, ".": 46, ",": 44, "/": 47, "-": 45}


class DAP:
    def __init__(self, port):
        self.sock = socket.create_connection(("localhost", port), 10)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(30)
        self.stream = self.sock.makefile("rb")
        self.seq = 0

    def request(self, command, arguments=None):
        self.seq += 1
        payload = json.dumps(dict(seq=self.seq, type="request", command=command,
                                  arguments=arguments or {})).encode()
        self.sock.sendall(b"Content-Length: %d\r\n\r\n" % len(payload) + payload)
        while True:
            headers = {}
            while True:
                line = self.stream.readline()
                if not line:
                    raise ConnectionError("Emulicious closed the DAP connection")
                if line == b"\r\n":
                    break
                key, value = line.decode().split(":", 1)
                headers[key.lower()] = value.strip()
            msg = json.loads(self.stream.read(int(headers["content-length"])))
            if msg.get("type") == "response" and msg.get("request_seq") == self.seq:
                if not msg.get("success"):
                    raise RuntimeError(msg)
                return msg.get("body", {})

    def number(self, expression):
        result = self.request("evaluate", dict(expression=expression, context="repl"))["result"]
        m = re.search(r"= \$([0-9A-Fa-f]+)", result)
        if not m:
            raise ValueError(result)
        return int(m.group(1), 16)

    def pause(self):
        self.request("pause", dict(threadId=1))

    def cont(self):
        self.request("continue", dict(threadId=1))


def load_symbols(map_path):
    text = map_path.read_text(errors="replace")
    return {name: int(value, 16) for value, name in
            re.findall(r"^\s*([0-9A-Fa-f]{8})\s+(_\w+)\s", text, re.M)}


def load_font():
    data = (ROOT / "assets" / "font6.c").read_text().split("{", 1)[1]
    raw = bytes(int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", data))
    return {raw[i * 8:i * 8 + 8]: chr(32 + i) for i in range(95)}


class Runner:
    def __init__(self, dap, symbols, font):
        self.dap = dap
        self.font = font
        self.wait_lo = symbols["_SG_waitForVBlank"]
        self.wait_hi = min(a for a in symbols.values() if a > self.wait_lo)
        self.game = symbols["_game"]

    def idle(self, timeout=60):
        """Run until the program sits in the frame wait (input / key wait)."""
        deadline = time.monotonic() + timeout
        hits = 0
        time.sleep(0.5)                 # let the last key be seen first
        while time.monotonic() < deadline:
            time.sleep(0.2)
            self.dap.pause()
            pc = self.dap.number("pc")
            hits = hits + 1 if self.wait_lo <= pc < self.wait_hi else 0
            if hits >= 3:               # drawing never waits for VBlank
                return
            self.dap.cont()
        raise TimeoutError("program did not become idle")

    def key(self, ch):
        code = JAVA_KEYS.get(ch)
        if code is None:
            code = ord(ch.upper())
        self.dap.cont()
        self.dap.request("keyboardEvent", dict(keyCode=code, action="press", duration=100))
        time.sleep(0.22)

    def type_line(self, text):
        for ch in text:
            self.key(ch)
        self.key("\n")
        self.idle()

    def vram(self, start, length):
        out = bytearray()
        for addr in range(start, start + length, 4):
            out += self.dap.number("dword.vram@@$%X" % addr).to_bytes(4, "little")
        return bytes(out)

    def patterns(self):
        return self.vram(0x0000, 6144)

    def colours(self):
        return self.vram(0x2000, 6144)

    def text_rows(self, pat, first=0, last=23):
        """40 columns of 6 pixel characters starting at x = 8"""
        rows = []
        for y in range(first, last + 1):
            line = ""
            for i in range(40):
                x = 8 + 6 * i
                glyph = []
                for r in range(8):
                    bits = 0
                    for c in range(6):
                        px = x + c
                        if pat[y * 256 + (px // 8) * 8 + r] & (0x80 >> (px % 8)):
                            bits |= 0x80 >> c
                    glyph.append(bits)
                line += self.font.get(bytes(glyph), "#")
            rows.append(line.rstrip())
        return rows

    def save_png(self, pat, path, col=None):
        """Graphics II screen; without the colour table white on black"""
        from PIL import Image
        img = Image.new("RGB", (256, 192))
        px = img.load()
        for y in range(192):
            for x in range(256):
                i = (y // 8) * 256 + (x // 8) * 8 + y % 8
                c = col[i] if col else 0xF1
                on = pat[i] & (0x80 >> (x % 8))
                px[x, y] = TMS_PALETTE[(c >> 4) if on else (c & 15)]
        img.resize((512, 384), Image.NEAREST).save(path)

    def room(self):
        # sGame: 13 flags, match_duration, int turn_count, current_room
        return self.dap.number("@$%X" % (self.game + 16))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", type=Path, default=ROOT / "mystery_house.SC")
    ap.add_argument("--map", type=Path, default=ROOT / "mystery_house.map")
    ap.add_argument("--port", type=int, default=58870)
    ap.add_argument("--commands", type=Path, help="one command per line")
    ap.add_argument("--shots", default="", help="comma separated command numbers to capture")
    ap.add_argument("--output", type=Path, default=ROOT / "test-output")
    ap.add_argument("--expect", help="text that must appear on screen at the end")
    args = ap.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    dap = DAP(args.port)
    dap.request("initialize", dict(clientID="mystery_house", adapterID="emulicious"))
    dap.request("launch", dict(program=str(args.rom.resolve()), stopOnEntry=False))
    dap.request("setExceptionBreakpoints", dict(filters=[]))
    dap.cont()
    run = Runner(dap, load_symbols(args.map), load_font())

    log = []
    run.idle()
    pat = run.patterns()
    run.save_png(pat, args.output / "00_title.png", run.colours())
    run.key(" ")                        # end splash
    run.idle()
    run.key(" ")                        # public domain page
    run.idle()
    run.type_line("g")
    commands = args.commands.read_text().split("\n") if args.commands else []
    commands = [c for c in commands if c.strip()]
    shots = {int(s) for s in args.shots.split(",") if s}
    shots.add(0)

    pat = run.patterns()
    run.save_png(pat, args.output / "01_start.png", run.colours())
    for n, cmd in enumerate(commands, 1):
        run.type_line(cmd)
        entry = dict(n=n, command=cmd, room=run.room())
        if n in shots or n == len(commands):
            pat = run.patterns()
            entry["text"] = run.text_rows(pat, 20)
            run.save_png(pat, args.output / ("%02d_%s.png" % (n, re.sub(r"\W+", "_", cmd))))
        log.append(entry)
        print(json.dumps(entry), flush=True)

    pat = run.patterns()
    screen = run.text_rows(pat)
    (args.output / "run.json").write_text(json.dumps(dict(log=log, screen=screen), indent=2))
    dap.pause()
    if args.expect:
        hist = " ".join(screen)
        ok = args.expect in hist
        print("expect %r: %s" % (args.expect, "OK" if ok else "MISSING"))
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
