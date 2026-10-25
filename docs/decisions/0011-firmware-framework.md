# 0011. ESP-IDF in C

Date: 2026-10-25
Status: Accepted

## Context

Options: Arduino core (via PlatformIO or Arduino IDE), PlatformIO + ESP-IDF,
or plain ESP-IDF.

## Decision

**ESP-IDF v5.3, written in C**, built with `idf.py`.

## Consequences

- Native FreeRTOS tasks, I²S, SPI master, RMT and SDSPI drivers.
- Closer to professional embedded practice than the Arduino core.
- Fewer ready-made libraries: the RC522 driver and the playback pipeline are
  written for this project. That's intended; it is a learning project.
- Hardware-independent logic (card state machine, tag parsing, brightness
  model, LED current limiter) is kept free of ESP-IDF headers so it builds and
  is unit-tested on a PC.
- Style: BARR-C:2018 inspired, fixed-width integer types.
