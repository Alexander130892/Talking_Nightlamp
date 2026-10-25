# Preparing the SD card

## Card and filesystem

- microSD ≤ 32 GB, formatted **FAT32** (cards up to 32 GB ship that way).
- A bedtime story at 96 kbit/s mono is ~0.7 MB per minute; 1 GB holds about
  24 hours of stories.

## Layout

```
/stories/
    001/
        01.mp3
        02.mp3        (optional: multi-part stories play in name order)
    002/
        01.mp3
    ...
/system/
    unknown.mp3       short neutral chime for unrecognised cards
```

Folder number = `story_id` written on the card
([card format](../firmware/card-format.md)). Use three digits.

Pre-load spare stories (e.g. folders 010–020) so follow-up cards given later
work without touching the SD card.

## Audio format

| Setting | Value |
|---|---|
| Codec | MP3 |
| Channels | Mono |
| Sample rate | 44.1 kHz |
| Bitrate | 96 kbit/s CBR |
| Loudness | Normalised to the same level across all files |
| Start / end | ~0.5 s silence, gentle fade-out |

Same loudness across files matters more than anything else: the volume knob
is limited, and one loud recording defeats that.

With [ffmpeg](https://ffmpeg.org):

```sh
ffmpeg -i input.wav -ac 1 -ar 44100 \
  -af "loudnorm=I=-20:TP=-3:LRA=11,afade=t=out:st=<end-3>:d=3" \
  -b:a 96k output.mp3
```

A batch script will live in [`tools/`](../../tools/).

## Recording family members

- Quiet room with soft furnishings; phone 20–30 cm from the mouth, slightly
  off-axis to avoid breath pops.
- Record in the phone's voice-memo app at the highest quality, export WAV or
  M4A, convert as above.
- One reader per card works well: the child learns whose voice is on which
  card.

Recordings are personal and are never committed to this repository
(`.gitignore` excludes `content/`, `*.mp3`, `*.wav`).
