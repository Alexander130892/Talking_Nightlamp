#!/usr/bin/env python3
"""
Generate the "unknown card" chime for the SD card: /system/unknown.mp3

A soft two-note bell (G5 -> E5, descending major third): a neutral
"not like that", deliberately not a buzzer. Synthesised from scratch, so
there is no licence question and anyone can regenerate the exact file.

Format matches docs/content/audio-content.md: MP3, mono, 44.1 kHz,
96 kbit/s CBR. Peak level is kept well below full scale so the chime is
never louder than the (loudness-normalised) stories.

Usage:
    python3 tools/make_chime.py [output.mp3]      (default: unknown.mp3)

Needs only the Python standard library plus ffmpeg on the PATH.
"""

import math
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

RATE = 44100            # Hz
DURATION_S = 0.95       # total length
PEAK_DBFS = -12.0       # loudest sample, relative to full scale
ATTACK_S = 0.004        # short ramp so the note start doesn't click
FADE_OUT_S = 0.15       # ramp to silence at the end

# (start time s, fundamental Hz, decay time constant s)
NOTES = [
    (0.00, 783.99, 0.22),   # G5
    (0.18, 659.25, 0.30),   # E5
]

# Bell-like timbre: fundamental plus a few partials (ratio, relative level).
# The inharmonic 2.76 partial is what makes it sound like a struck bar.
PARTIALS = [(1.0, 1.0), (2.0, 0.25), (2.76, 0.12), (4.0, 0.05)]


def note_sample(t, f0, tau):
    """One struck note at time t (s) after its start."""
    if t < 0.0:
        return 0.0
    env = math.exp(-t / tau)
    if t < ATTACK_S:
        env *= t / ATTACK_S
    return env * sum(level * math.sin(2.0 * math.pi * f0 * ratio * t)
                     for ratio, level in PARTIALS)


def synthesise():
    n = int(RATE * DURATION_S)
    samples = []
    for i in range(n):
        t = i / RATE
        s = sum(note_sample(t - start, f0, tau) for start, f0, tau in NOTES)
        remaining = DURATION_S - t
        if remaining < FADE_OUT_S:
            s *= remaining / FADE_OUT_S
        samples.append(s)

    peak = max(abs(s) for s in samples)
    gain = (10.0 ** (PEAK_DBFS / 20.0)) / peak
    return [int(round(s * gain * 32767.0)) for s in samples]


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(struct.pack("<%dh" % len(samples), *samples))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "unknown.mp3"
    if shutil.which("ffmpeg") is None:
        sys.exit("ffmpeg not found (macOS: brew install ffmpeg)")

    with tempfile.NamedTemporaryFile(suffix=".wav") as tmp:
        write_wav(tmp.name, synthesise())
        subprocess.run(
            ["ffmpeg", "-y", "-loglevel", "error", "-i", tmp.name,
             "-ac", "1", "-ar", str(RATE), "-codec:a", "libmp3lame",
             "-b:a", "96k", "-write_xing", "0", out],
            check=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
