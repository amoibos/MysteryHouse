#!/usr/bin/env python3
"""Make a VGM from midi2vgm.py readable for PSGlib's vgm2psg.

vgm2psg does not know the short wait commands 0x7n ("wait n+1 samples"),
which midi2vgm writes.  They are turned into 0x61 waits, consecutive waits
are merged; GD3 tags and the loop point are dropped.

vgm2psg also rounds every single wait down to whole frames (735 samples)
and drops the rest, so the voices drift apart.  Every write is therefore
moved to the frame nearest to its absolute time and the waits become
exact multiples of a frame.

usage: vgm_fix.py in.vgm out.vgm
"""

import struct
import sys

FRAME = 735             # samples per 60 Hz frame


def fix(data):
    version = struct.unpack_from("<I", data, 0x08)[0]
    rel = struct.unpack_from("<I", data, 0x34)[0] if version >= 0x150 else 0
    start = 0x34 + rel if rel else 0x40
    out, now, done, p = bytearray(), 0, 0, start

    def flush():
        nonlocal done
        wait = (now + FRAME // 2) // FRAME * FRAME - done
        done += wait
        while wait > 0:
            n = min(wait, 0xFFFF // FRAME * FRAME)
            out.extend(struct.pack("<BH", 0x61, n))
            wait -= n

    while p < len(data):
        cmd = data[p]
        if 0x70 <= cmd <= 0x7F:
            now += (cmd & 15) + 1
            p += 1
        elif cmd == 0x61:
            now += struct.unpack_from("<H", data, p + 1)[0]
            p += 3
        elif cmd == 0x62:
            now += 735
            p += 1
        elif cmd == 0x63:
            now += 882
            p += 1
        elif cmd in (0x4F, 0x50):
            flush()
            out += data[p:p + 2]
            p += 2
        elif cmd == 0x66:
            break
        else:
            raise SystemExit("unsupported VGM command 0x%02X at 0x%X" % (cmd, p))
    flush()
    out.append(0x66)

    header = bytearray(data[:start])
    struct.pack_into("<I", header, 0x14, 0)                 # no GD3
    struct.pack_into("<I", header, 0x1C, 0)                 # no loop
    struct.pack_into("<I", header, 0x20, 0)
    struct.pack_into("<I", header, 0x04, len(header) + len(out) - 4)
    return bytes(header + out)


if __name__ == "__main__":
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    with open(sys.argv[2], "wb") as f:
        f.write(fix(data))
