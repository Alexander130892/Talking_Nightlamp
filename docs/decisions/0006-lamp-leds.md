# 0006. SK6812 RGBW warm white, buffered by a 74AHCT125

Date: 2026-10-25
Status: Accepted

## Context

The lamp needs a warm, even white at night-light levels, plus per-card colour
accents.

## Decision

- **12 × SK6812 RGBW, warm white ~3000 K** (cut from a 60 LED/m strip and
  wrapped around a printed cylinder inside the dome).
- Data line buffered through a **74AHCT125** powered from 5 V.
- Driven by the ESP32 **RMT** peripheral.

## Why RGBW instead of WS2812B RGB

Mixed RGB white is cold and uneven, and at low brightness each colour steps
separately. A dedicated warm-white die gives a proper night-light colour. The
cost difference was a few euros. The SK6812 also holds its colour better when
the supply voltage sags.

## Why a level shifter

The SK6812/WS2812 input threshold V_IH is 0.7 × VDD = 3.5 V at 5 V. The ESP32
drives 3.3 V, which is out of spec. It often works, but this is a gift that
cannot be debugged once handed over.

The 74AHCT125 has TTL input thresholds (V_IH = 2.0 V) and outputs a full 5 V.
Bidirectional BSS138 "level shifter" boards were rejected: they rely on a
pull-up to drive the line high, and that rise time is too slow for 800 kbit/s
data with ~400 ns pulses.

## Why RMT

Bit timing is fixed by the LED (0 ≈ 0.4 µs high, 1 ≈ 0.8 µs high, ±150 ns).
Bit-banging would be disturbed by interrupts while I²S streams audio. RMT
generates the waveform in hardware.

## Consequences

- 32 bits per LED, GRBW order. One frame for 12 LEDs ≈ 0.5 ms + latch.
- Full white on all four dies ≈ 80 mA per LED. Bounded by the current limiter
  in [0008](0008-power-architecture.md).
- Layout: bulk capacitor at the strip, 330 Ω series resistor in the data line
  close to the first LED.
