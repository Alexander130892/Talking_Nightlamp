# 0013. minimp3 as MP3 decoder

Date: 2026-10-29
Status: Accepted

## Context

[0002](0002-audio-path.md) decided to decode MP3 on the ESP32 and stream
PCM over I²S. The ESP32 has no MP3 hardware, so a software decoder is needed.
Requirements: plain C, fits in RAM next to SD, RFID and LED buffers, licence
compatible with the MIT/CC BY-SA repository, and readable enough that the
pipeline stays understood (the reason for 0002).

## Decision

Use **minimp3** (https://github.com/lieff/minimp3), vendored unmodified as the
ESP-IDF component `firmware/components/minimp3`, built with `-O2` and
`MINIMP3_ONLY_MP3`.

## Alternatives considered

| Option | Why not |
|---|---|
| libhelix-mp3 | Fast fixed-point decoder, but RealNetworks RPSL/RCSL licence is awkward for an open project |
| ESP-ADF | Complete audio framework, but large and hides the pipeline this project wants to own |

## Consequences

- CC0 (public domain): no licence conditions on the repository.
- RAM: `mp3dec_t` ≈ 6.7 KB (static) plus input and PCM buffers (~17 KB).
- `mp3dec_decode_frame()` keeps a ~16 KB scratch struct **on the stack**:
  the audio task needs ≥ 24 KB of stack (bring-up uses 32 KB).
- Decoder output is 16-bit PCM per channel; stereo files are downmixed to mono
  because the MAX98357A plays one channel.
- CPU load measured in bring-up 08b (test plan C4).
