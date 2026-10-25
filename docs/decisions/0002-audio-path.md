# 0002. MP3 decode on the ESP32, I²S to a MAX98357A

Date: 2026-10-25
Status: Accepted

## Context

Two proven approaches exist for this kind of box:

- **DFPlayer Mini**: a separate MP3 player module with its own SD slot,
  controlled over UART (TonUINO).
- **Decode on the MCU**: read MP3 from SD, decode in software, stream PCM over
  I²S to a DAC/amplifier (ESPuino).

## Decision

Decode on the ESP32 and stream over **I²S to a MAX98357A** class-D amplifier.

## Alternatives considered

**DFPlayer Mini.** Simplest to get working, but:

- Many clone chips (YX5200, MH2024K, GD3200B) differ in command behaviour and
  busy-pin timing.
- Audible pop at power-on.
- Can pause and resume within a session but cannot reliably report the
  playback position.
- The audio pipeline is a black box.

## Consequences

- Full control of the pipeline: fade-in/out, volume ramping, mute at boot,
  exact playback position.
- More firmware to write: SD access, MP3 decoder integration, I²S buffering.
- Originally the deciding argument was persistent resume across power cycles.
  The requirements later dropped that (a paused story restarts after a timeout
  or power cycle, see F6). I²S was kept for the remaining reasons: no pop, no
  clone quirks, and owning the pipeline.
