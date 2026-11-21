#!/usr/bin/env python3
"""
make_jingles.py - fairy-tale intro and outro jingles for the stories.

    python3 tools/make_jingles.py OUT_DIR

Writes OUT_DIR/intro.wav, outro.wav (for GarageBand) and intro.mp3,
outro.mp3 (lamp-ready: mono, 44.1 kHz, 96 kbit/s).

Synthesised from scratch (standard library + ffmpeg for reverb, loudness and
MP3), so the result is an original work with no licence attached, and the
repository only needs this script.

Sound:
  - celesta: bell-like, a few slightly inharmonic partials, quick decay
  - harp:    plucked, harmonic partials, longer decay
  - pad:     soft sustained chord underneath
  - sparkle: very quiet high bell grains (intro only)
Intro  (~6 s): pad on C, rising harp glissando, a short celesta motif that
               ends high and open ("once upon a time...").
Outro  (~8 s): falling celesta motif, slow falling harp, pad F -> C (a soft
               "amen" cadence) and a last low bell fading out ("good night").
Both are mixed softer than speech (LOUDNESS_LUFS), with gentle fades.
"""

import math
import os
import random
import struct
import subprocess
import sys
import wave

RATE = 44100
LOUDNESS_LUFS = -20.0      # stories are -16: the jingle stays a bit softer
SEED = 7                   # fixed: the same jingle every run

NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def freq(name):
    """'C5' -> Hz (A4 = 440)."""
    semis = NOTE[name[0]] + 12 * (int(name[1:]) + 1)
    return 440.0 * 2.0 ** ((semis - 69) / 12.0)


class Track:
    def __init__(self, seconds):
        self.buf = [0.0] * int(seconds * RATE)

    def add(self, start_s, samples):
        i0 = int(start_s * RATE)
        for k, v in enumerate(samples):
            if i0 + k < len(self.buf):
                self.buf[i0 + k] += v


def tone(f, dur, partials, attack=0.003, gain=1.0):
    """partials: list of (ratio, amplitude, decay_seconds)."""
    n = int(dur * RATE)
    a_n = max(1, int(attack * RATE))
    out = [0.0] * n
    for ratio, amp, tau in partials:
        w = 2.0 * math.pi * f * ratio / RATE
        if f * ratio > RATE / 2.2:
            continue
        for i in range(n):
            out[i] += amp * math.sin(w * i) * math.exp(-i / (tau * RATE))
    for i in range(min(a_n, n)):
        out[i] *= i / a_n
    r_n = min(n, int(0.08 * RATE))           # end every note softly: no click
    for i in range(r_n):
        out[n - r_n + i] *= (r_n - i) / r_n
    return [gain * v for v in out]


def celesta(f, dur=2.5, gain=0.35):
    return tone(f, dur, [(1.0, 1.0, 1.1), (2.0, 0.35, 0.5),
                         (3.01, 0.12, 0.25), (4.17, 0.08, 0.12)], gain=gain)


def harp(f, dur=3.0, gain=0.28):
    return tone(f, dur, [(n, 1.0 / n, 2.2 / (1 + 0.4 * n)) for n in range(1, 7)],
                attack=0.002, gain=gain)


def pad(freqs, dur, gain=0.06, attack=0.8, release=1.2):
    n = int(dur * RATE)
    out = [0.0] * n
    for f in freqs:
        w = 2.0 * math.pi * f / RATE
        for i in range(n):
            out[i] += math.sin(w * i) + 0.2 * math.sin(3 * w * i)
    a_n, r_n = int(attack * RATE), int(release * RATE)
    for i in range(n):
        env = min(1.0, i / a_n) * min(1.0, (n - i) / r_n)
        out[i] *= gain * env
    return out


def sparkle(track, start, end, rnd, count=14):
    tops = [freq(x) for x in ("C7", "D7", "E7", "G7", "A7")]
    for _ in range(count):
        t = rnd.uniform(start, end)
        track.add(t, tone(rnd.choice(tops), 0.6, [(1.0, 1.0, 0.15)], gain=0.05))


def fade_out(buf, seconds):
    n = int(seconds * RATE)
    for i in range(n):
        buf[len(buf) - n + i] *= (n - i) / n


def make_intro(rnd):
    t = Track(6.0)
    t.add(0.0, pad([freq("C4"), freq("E4"), freq("G4")], 6.0))
    for k, note in enumerate(["C4", "E4", "G4", "C5", "E5", "G5", "C6"]):
        t.add(0.15 + 0.09 * k, harp(freq(note)))
    for k, note in enumerate(["E5", "G5", "A5", "G5"]):
        t.add(1.3 + 0.36 * k, celesta(freq(note)))
    t.add(2.8, celesta(freq("D6"), dur=3.0, gain=0.3))
    t.add(2.8, celesta(freq("G5"), dur=3.0, gain=0.18))
    sparkle(t, 2.9, 4.6, rnd)
    fade_out(t.buf, 1.6)
    return t.buf


def make_outro(rnd):
    t = Track(8.0)
    t.add(0.0, pad([freq("F3"), freq("A3"), freq("C4")], 3.6, release=1.0))
    t.add(3.0, pad([freq("C3"), freq("E3"), freq("G3")], 5.0, attack=1.0, release=2.5))
    for k, note in enumerate(["E6", "D6", "C6", "A5", "G5"]):
        t.add(0.2 + 0.42 * k, celesta(freq(note), gain=0.3))
    for k, note in enumerate(["C6", "G5", "E5", "C5", "G4", "E4", "C4"]):
        t.add(2.4 + 0.16 * k, harp(freq(note), gain=0.22))
    t.add(4.2, celesta(freq("C5"), dur=3.8, gain=0.3))
    t.add(4.2, celesta(freq("C6"), dur=3.8, gain=0.12))
    fade_out(t.buf, 2.5)
    return t.buf


def write_wav(path, buf):
    peak = max(abs(v) for v in buf) or 1.0
    scale = 0.5 / peak                      # -6 dBFS raw; ffmpeg sets loudness
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(v * scale * 32767)) for v in buf))


def finish(raw, wav_out, mp3_out):
    """Soft reverb, loudness, and the two output formats."""
    chain = ("aecho=0.8:0.6:45|90|150:0.30|0.20|0.12,"
             "loudnorm=I=%.1f:TP=-3:LRA=11" % LOUDNESS_LUFS)
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", raw, "-af", chain,
                    "-ar", str(RATE), "-ac", "1", wav_out], check=True)
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", wav_out,
                    "-ac", "1", "-ar", str(RATE), "-b:a", "96k", mp3_out], check=True)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    rnd = random.Random(SEED)
    for name, make in (("intro", make_intro), ("outro", make_outro)):
        raw = os.path.join(out, name + "_raw.wav")
        write_wav(raw, make(rnd))
        finish(raw, os.path.join(out, name + ".wav"), os.path.join(out, name + ".mp3"))
        os.remove(raw)
        print("wrote", os.path.join(out, name + ".wav / .mp3"))


if __name__ == "__main__":
    main()
