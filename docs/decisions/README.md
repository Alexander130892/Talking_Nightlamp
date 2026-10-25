# Architecture decision records

Each file records one decision: the context, what was chosen, what was
rejected, and the consequences. Records are never rewritten; a changed decision
gets a new record that supersedes the old one.

| # | Decision | Status |
|---|---|---|
| [0001](0001-microcontroller.md) | ESP32-WROOM-32 as microcontroller | Accepted |
| [0002](0002-audio-path.md) | MP3 decode on the ESP32, I²S to a MAX98357A | Accepted |
| [0003](0003-storage-no-wifi.md) | microSD only, no WiFi | Accepted |
| [0004](0004-card-detection.md) | Presence polling with WUPA and a 3 s grace period | Accepted |
| [0005](0005-card-data-on-tag.md) | Story number and colour stored on the tag | Accepted |
| [0006](0006-lamp-leds.md) | SK6812 RGBW warm white, buffered by a 74AHCT125 | Accepted |
| [0007](0007-low-brightness-dimming.md) | Dim below one PWM step by switching LEDs off | Accepted |
| [0008](0008-power-architecture.md) | Dedicated USB-C input feeding VIN, star ground | Accepted |
| [0009](0009-volume-limiting.md) | Volume limited by speaker impedance and amplifier gain | Accepted |
| [0010](0010-sourcing.md) | Parts from one EU shop instead of AliExpress | Accepted |
| [0011](0011-firmware-framework.md) | ESP-IDF in C | Accepted |

Template:

```
# NNNN. Title
Date: YYYY-MM-DD
Status: Proposed | Accepted | Superseded by NNNN

## Context
## Decision
## Alternatives considered
## Consequences
```
