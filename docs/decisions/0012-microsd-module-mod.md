# 0012. microSD module: 5 V supply, 1 kΩ series resistors bridged

Date: 2026-10-27
Status: Accepted

## Context

The ordered microSD module is the DFRobot Fermion DFR0229 (schematic
"MicroSD Module V1.0", 2013). It was chosen in [0010](0010-sourcing.md)
assuming it accepted 3.3 V logic. The schematic shows:

- VCC (+5) feeds a **BL8555-33** LDO that powers the card at 3.3 V.
- SCK, MOSI and SS each pass a **1 kΩ series resistor** (R33–R35) into a
  node with **2.2 kΩ to GND** (R30–R32): a divider for 5 V logic.
- MISO goes from the card straight to the header.

With the ESP32's 3.3 V outputs, the card would see
3.3 V × 2.2 / 3.2 = **2.27 V**, against an SD input threshold of
V_IH = 0.625 × 3.3 V = **2.06 V**: about 0.2 V margin, less under load.
Too little for a device that has to work for years in a child's room.

## Decision

- Supply the module from **+5 V** (its LDO needs the headroom).
- **Bridge R33, R34 and R35 with solder.** The card inputs are then driven
  directly by the ESP32 at 3.3 V; the 2.2 kΩ resistors remain as a 1.5 mA
  load per pin when high, which a GPIO drives easily.
- Label the module "3V3 LOGIC ONLY".

## Alternatives considered

| Option | Why not |
|---|---|
| Use the module unmodified | ~0.2 V logic margin |
| 330 Ω in parallel with each 1 kΩ (≈ 250 Ω, card sees ≈ 2.96 V) | Keeps some damping, but fiddly through-hole-on-SMD soldering |
| Reorder a plain 3.3 V microSD breakout | Works, costs a reorder and delivery time; still an option if the mod fails |

## Consequences

- Logic high at the card ≈ 3.3 V: margin ~1.2 V instead of ~0.2 V.
- No series damping: keep the SPI wires short. MP3 at 96 kbit/s needs only
  ~12 kB/s, so the SPI clock can stay modest.
- The modified module must **never** be used with a 5 V microcontroller:
  5 V would reach the card inputs directly.
- Reversible: removing the solder blobs restores the original module.
- Verified by measurement, see test plan row C0.
