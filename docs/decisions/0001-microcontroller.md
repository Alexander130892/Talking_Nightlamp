# 0001. ESP32-WROOM-32 as microcontroller

Date: 2026-10-25
Status: Accepted

## Context

The box must decode MP3 audio, read an SPI RFID reader and an SD card, drive
an LED chain with sub-microsecond timing, and read buttons, all at once.
It must survive having its power pulled at any moment.

Available on the bench: ESP8266 boards (D1 mini, NodeMCU), ATtiny,
Arduino Nano / LGT8 Nano, Raspberry Pi Zero 2, ESP32-WROOM-32 30-pin USB-C
devkit.

## Decision

Use the **ESP32-WROOM-32 30-pin devkit** (no PSRAM).

- 520 KB SRAM: an MP3 decoder needs ~30–40 KB plus I²S and SD buffers.
- Two free SPI hosts, so RFID and SD can run on separate buses.
- I²S and RMT peripherals in hardware.
- About 14 GPIOs needed, available on the 30-pin board.

## Alternatives considered

| Option | Why not |
|---|---|
| ESP8266 boards | Not enough RAM for decode + SD + I²S at once; too few free GPIOs |
| ATtiny, Nano, LGT8 | Can't decode MP3 |
| Raspberry Pi Zero 2 | Linux: 20–30 s boot and SD-card corruption risk when unplugged mid-story |

## Consequences

- GPIO 6–11 are wired to flash and unusable.
- GPIO 34–39 are input-only and have no internal pull-ups.
- Strapping pins (0, 2, 5, 12, 15) must not be held in the wrong state at
  boot. GPIO 12 in particular must not be pulled high.
- The devkit's regulator is an AMS1117-3.3 (~1 V dropout), see
  [0008](0008-power-architecture.md).
