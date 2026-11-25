#!/usr/bin/env python3
"""Title music: MIDI -> SN76489 VGM -> PSGlib data -> assets/music.c

  1. arrange for the 3 tone voices of the PSG: every --voice turns one or
     more MIDI channels (0+10+11) into a single line (top / bottom / last
     note of a chord, played with the instrument of the first channel),
     optionally transposed; other tone channels are dropped, drums (MIDI
     channel 9) stay unless --no-drums.  Notes below A2 (110 Hz, the
     lowest PSG tone) are moved up by octaves, so transpose a bass part as
     a whole instead.
  2. the tempo is rounded so that a 16th note (--grid) lasts whole 60 Hz
     frames, otherwise the notes alternate e.g. 12/12/13 frames and limp
  3. --start: begin at the bar line nearest to it (the first time signature
     gives the bar length); --seconds: cut at the last bar line before it
     (time signature changes are taken as bar lines), hanging notes are
     released
  4. midi2vgm.py (../midi2vgm) maps the voices to the PSG + noise drums
  5. vgm_fix.py rewrites the short 0x7n waits that vgm2psg does not know
  6. vgm2psg + psgcomp (PSGlib tools) convert and compress

usage (the Mystery House title music, Saint-Saens: Danse macabre):
  python tools/make_music.py assets/midi/danse_macabre.mid --grid 2 --voice 10+0+11:top --voice 12+2+13:top --voice 15+14+8+3:bottom:12 --no-drums
"""

import argparse
import os
import subprocess
import sys
import tempfile

import mido

import vgm_fix

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# external tools: midi2vgm (github.com/amoibos/midi2vgm) and the PSGlib
# converters vgm2psg / psgcomp (devkitSMS), found via --midi2vgm and
# --psg-tools, the environment variables MIDI2VGM / PSG_TOOLS or the PATH
MIDI2VGM = os.environ.get("MIDI2VGM", os.path.join(ROOT, "..", "midi2vgm", "midi2vgm.py"))
PSG_TOOLS = os.environ.get("PSG_TOOLS", "")
DRUMS = 9
LOWEST_NOTE = 45        # A2, 110 Hz: tone period 1017 of max. 1023


def absolute(track):
    t = 0
    for msg in track:
        t += msg.time
        yield t, msg


def to_track(events):
    track, last = mido.MidiTrack(), 0
    for t, msg in sorted(events, key=lambda e: e[0]):
        track.append(msg.copy(time=t - last))
        last = t
    track.append(mido.MetaMessage("end_of_track", time=0))
    return track


def mono_line(events, mode, transpose):
    """note events of one channel -> one note at a time"""
    out, cur, i = [], None, 0       # cur: (played note, original note)
    notes = sorted((e for e in events if e[1].type in ("note_on", "note_off")),
                   key=lambda e: (e[0], e[1].type == "note_on" and e[1].velocity > 0))
    while i < len(notes):
        t = notes[i][0]
        group = []
        while i < len(notes) and notes[i][0] == t:
            group.append(notes[i][1])
            i += 1
        ons = [m for m in group if m.type == "note_on" and m.velocity]
        for m in group:
            if m not in ons and cur and m.note == cur[1]:
                out.append((t, m.copy(note=cur[0])))
                cur = None
        if ons:
            pick = {"top": max(ons, key=lambda m: m.note), "bottom": min(ons, key=lambda m: m.note),
                    "last": ons[-1]}[mode]
            note = pick.note + transpose
            while note < LOWEST_NOTE:
                note += 12
            if cur:
                out.append((t, mido.Message("note_off", channel=pick.channel, note=cur[0], velocity=0)))
            out.append((t, pick.copy(note=note)))
            cur = (note, pick.note)
    return out


def arrange(src, dst, voices, drums=True):
    m = mido.MidiFile(src)
    meta, by_channel = [], {}
    for track in m.tracks:
        for t, msg in absolute(track):
            if msg.is_meta:
                if msg.type != "end_of_track":
                    meta.append((t, msg))
            elif hasattr(msg, "channel"):
                by_channel.setdefault(msg.channel, []).append((t, msg))
    out = mido.MidiFile(type=1, ticks_per_beat=m.ticks_per_beat)
    out.tracks.append(to_track(meta))
    for channels, mode, transpose in voices:
        # all notes of a voice go to its first channel (its instrument)
        ch = channels[0]
        events = [e for c in channels for e in by_channel.get(c, [])]
        other = [e for e in by_channel.get(ch, []) if e[1].type not in ("note_on", "note_off")]
        line = [(t, msg.copy(channel=ch)) for t, msg in mono_line(events, mode, transpose)]
        out.tracks.append(to_track(other + line))
    if drums and DRUMS in by_channel:
        out.tracks.append(to_track(by_channel[DRUMS]))
    out.save(dst)


def snap_tempo(src, dst, grid):
    """PSGlib plays in 60 Hz frames: with 12.33 frames per eighth the notes
    alternate 12/12/13 frames and the rhythm limps.  Every tempo is changed
    slightly so that 1/grid beat lasts a whole number of frames."""
    m = mido.MidiFile(src)
    for track in m.tracks:
        for i, msg in enumerate(track):
            if msg.type == "set_tempo":
                frames = max(1, round(msg.tempo * 60 / 1e6 / grid)) * grid
                track[i] = msg.copy(tempo=round(frames * 1e6 / 60))
    m.save(dst)


def trim_start(src, dst, seconds):
    """start the song at the bar line nearest to seconds: notes before it are
    dropped, tempo/program/controller settings move to the new start"""
    m = mido.MidiFile(src)
    tpb, tempo, bar = m.ticks_per_beat, 500000, m.ticks_per_beat * 4
    for t, msg in absolute(m.tracks[0]):
        if t == 0 and msg.type == "set_tempo":
            tempo = msg.tempo
        if t == 0 and msg.type == "time_signature":
            bar = tpb * msg.numerator * 4 // msg.denominator
    start = round(mido.second2tick(seconds, tpb, tempo) / bar) * bar
    for i, track in enumerate(m.tracks):
        events, dropped = [], set()
        for t, msg in absolute(track):
            if msg.type == "end_of_track":
                continue
            if msg.type in ("note_on", "note_off"):
                key = (msg.channel, msg.note)
                if t < start:
                    if msg.type == "note_on" and msg.velocity:
                        dropped.add(key)
                    else:
                        dropped.discard(key)
                    continue
                if key in dropped and not (msg.type == "note_on" and msg.velocity):
                    dropped.discard(key)
                    continue
            events.append((max(t - start, 0), msg))
        m.tracks[i] = to_track(events)
    m.save(dst)
    return mido.tick2second(start, tpb, tempo)


def parse_voice(text):
    parts = text.split(":")
    return ([int(c) for c in parts[0].split("+")], parts[1] if len(parts) > 1 else "top",
            int(parts[2]) if len(parts) > 2 else 0)


def cut_midi(src, dst, seconds):
    m = mido.MidiFile(src)
    tpb, tempo, t, bars = m.ticks_per_beat, 500000, 0, [0]
    for msg in m.tracks[0]:
        t += msg.time
        if msg.type == "set_tempo" and t == 0:
            tempo = msg.tempo
        if msg.type == "time_signature":
            bars.append(t)
    cut = max(b for b in bars if mido.tick2second(b, tpb, tempo) <= seconds)
    out = mido.MidiFile(type=1, ticks_per_beat=tpb)
    for track in m.tracks:
        new, t, last, sounding = mido.MidiTrack(), 0, 0, set()
        for msg in track:
            t += msg.time
            if t >= cut:
                break
            if msg.type == "note_on" and msg.velocity:
                sounding.add((msg.channel, msg.note))
            elif msg.type in ("note_on", "note_off"):
                sounding.discard((msg.channel, msg.note))
            new.append(msg.copy(time=t - last))
            last = t
        for ch, note in sorted(sounding):
            new.append(mido.Message("note_off", channel=ch, note=note, velocity=0, time=cut - last))
            last = cut
        new.append(mido.MetaMessage("end_of_track", time=cut - last))
        out.tracks.append(new)
    out.save(dst)
    return mido.tick2second(cut, tpb, tempo)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("midi")
    ap.add_argument("--voice", action="append", type=parse_voice, default=[],
                    help="CHANNEL[+CHANNEL...][:top|bottom|last[:TRANSPOSE]], first = highest priority")
    ap.add_argument("--no-drums", action="store_true", help="leave out the noise channel drums")
    ap.add_argument("--start", type=float, help="begin at the bar line nearest to this second")
    ap.add_argument("--seconds", type=float, help="cut the song (default: complete)")
    ap.add_argument("--grid", type=int, default=4,
                    help="beat subdivision that gets whole frames (default 4 = 16th notes)")
    ap.add_argument("--midi2vgm", default=MIDI2VGM, help="path of midi2vgm.py")
    ap.add_argument("--psg-tools", default=PSG_TOOLS,
                    help="directory of vgm2psg and psgcomp (default: PATH)")
    ap.add_argument("--out", default=os.path.join(ROOT, "assets", "music.c"))
    args = ap.parse_args()

    tmp = tempfile.mkdtemp(prefix="mh_music_")
    song, vgm, fixed, psg, comp = (os.path.join(tmp, n) for n in
                                   ("song.mid", "raw.vgm", "fixed.vgm", "raw.psg", "music.psg"))
    src = args.midi
    if args.voice:
        arrange(src, song, args.voice, not args.no_drums)
        src = song
    snap_tempo(src, song, args.grid)
    if args.start:
        print("start at bar line %.1f s" % trim_start(song, song, args.start))
    if args.seconds:
        length = cut_midi(song, song, args.seconds)
    else:
        length = mido.MidiFile(song).length
    cmd = [sys.executable, args.midi2vgm, song, vgm]
    if args.voice:
        cmd += ["--channels", ",".join(str(chs[0]) for chs, _, _ in args.voice)]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
    with open(vgm, "rb") as f:
        data = vgm_fix.fix(f.read())
    with open(fixed, "wb") as f:
        f.write(data)
    subprocess.run([os.path.join(args.psg_tools, "vgm2psg"), fixed, psg], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([os.path.join(args.psg_tools, "psgcomp"), psg, comp], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(comp, "rb") as f:
        music = f.read()

    name = os.path.basename(args.midi)
    with open(args.out, "w", newline="\n") as f:
        f.write("/* generated by tools/make_music.py from %s (%.1f s) - do not edit */\n\n"
                % (name, length))
        f.write("const unsigned char title_music[%d] = {\n" % len(music))
        for i in range(0, len(music), 16):
            f.write("    " + ",".join("0x%02x" % b for b in music[i:i + 16]) + ",\n")
        f.write("};\n")
    print("%s: %.1f s -> %d bytes" % (name, length, len(music)))


if __name__ == "__main__":
    main()
