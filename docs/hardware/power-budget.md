# Power budget

Supply: 5 V USB-C, CE-marked adapter ≥ 1 A (ships with 5.1 V / 3 A).
Architecture: see [ADR 0008](../decisions/0008-power-architecture.md).

| Load | Worst case | Capped by design | How it's capped |
|---|---|---|---|
| ESP32 @ 240 MHz, radio off, I²S + SD | ~80 mA (SD write peaks ~100 mA) | ~100 mA | — |
| RC522, field on | ~30 mA | ~30 mA | — |
| 12 × SK6812 RGBW, all four dies full | ~960 mA | **150 mA** | Firmware current limiter |
| MAX98357A | ~0.8 A peaks (4 Ω, 3 W) | **~0.3 A peaks** | 8 Ω speaker + gain |
| **Total** | **~2.0 A** | **≈ 0.6 A, peak ≤ 0.8 A** | |

## Per-LED current (SK6812 RGBW, ~20 mA per die at full)

| Colour | Dies | Current at full |
|---|---|---|
| Warm white (W) | 1 | ~20 mA |
| Saturated R, G or B | 1 | ~20 mA |
| Yellow, cyan, purple | 2 | ~40 mA |
| RGB white | 3 | ~60 mA |
| All on | 4 | ~80 mA |

The limiter estimates current per frame as the sum over all channels of
`value / 255 × 20 mA` and scales the whole frame if it exceeds the budget.
Colour therefore doesn't need special rules: any effect is safe by
construction.

## Supply path checks

- AMS1117-3.3 dropout ~1 V → needs ≥ 4.3 V at VIN. Feeding VIN directly
  (bypassing the devkit's Schottky) gives ~5.0 V.
- Bulk capacitance: 1000 µF at the star point, 470 µF at the LED strip.
- Polyfuse 1.0 A hold at the input. Hold current derates with temperature
  (roughly 15–20 % lower at 50 °C inside a closed enclosure). Audio peaks are
  milliseconds long and a polyfuse needs seconds of overload to trip, so this
  is fine as long as the *average* stays near 0.6 A. If it ever trips in use,
  move to a 1.35 A part.
