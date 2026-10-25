# 0008. Dedicated USB-C input feeding VIN, star ground

Date: 2026-10-25
Status: Accepted

## Context

Measured on the devkit used:

- The 3.3 V regulator is an AMS1117-3.3 (marking "1117 33"), ~1 V dropout.
- A Schottky diode sits between USB VBUS and the **VIN** pin: VIN read 4.8 V
  with the board powered from USB; in-circuit diode test read ~0.2 V forward.
- The board's USB-C port has CC pull-downs (it powers from a USB-C PD charger).

Audio peaks are short and sharp. If they sag the 5 V rail, the AMS1117 drops
out and the ESP32 brown-out detector resets the board mid-story.

## Decision

- A separate **USB-C power breakout** in the enclosure wall, with
  **5.1 kΩ pull-downs on CC1 and CC2** (otherwise a USB-C charger supplies no
  5 V at all).
- 5 V goes through a **polyfuse** to a **star point with 1000 µF**, then
  separately to the amplifier, the LED strip (+470 µF at the strip), the
  74AHCT125 and the devkit's **VIN** pin. All grounds return to the star point.
- The devkit's own USB port is used only for flashing.
- Firmware **current limiter** for the LEDs: budget 150 mA, frame scaled down
  if exceeded.
- Ships with a CE-marked 5.1 V / 3 A USB-C adapter.

## Consequences

- Feeding VIN bypasses the diode: the AMS1117 sees ~5.0 V instead of ~4.8 V.
- With 5 V on VIN, the diode is reverse-biased towards the laptop, so flashing
  with the external supply connected is safe **on this board**. Measure your
  own board first, see the test plan.
- Design load ≈ 0.55–0.65 A, peak ≤ 800 mA. See
  [power budget](../hardware/power-budget.md).
