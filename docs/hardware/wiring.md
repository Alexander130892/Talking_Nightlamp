# Wiring

A KiCad schematic will follow in [`hardware/schematic/`](../../hardware/schematic/).
Until then, this netlist is the reference. GPIO numbers follow
[pin-mapping.md](pin-mapping.md).

## Block diagram

```
USB-C breakout ──► polyfuse ──► STAR (+5V, 1000 µF) ─┬─► MAX98357A VIN ──► speaker 8 Ω
 (CC1, CC2 → 5.1 kΩ → GND)                          ├─► SK6812 strip 5V (+470 µF)
                                                     ├─► 74AHCT125 VCC (+100 nF)
                                                     └─► ESP32 devkit VIN ──► AMS1117 ──► 3V3
                                                                                ├─► RC522 3.3V
                                                                                └─► microSD 3V3
GND: every module returns to the STAR point.
```

## Power nets

| Net | Connects |
|---|---|
| VBUS_IN | USB-C breakout VBUS → polyfuse pin 1 |
| +5V | polyfuse pin 2, 1000 µF (+), MAX98357A VIN, strip 5V, 470 µF (+) at strip, 74AHCT125 pin 14, ESP32 VIN |
| +3V3 | ESP32 3V3 pin, RC522 3.3V, microSD 3V3, encoder pull-ups |
| GND | USB-C GND, both capacitors (−), all module GNDs, 74AHCT125 pin 7 |
| CC1 / CC2 | each through its own 5.1 kΩ to GND |

**Never connect the RC522 to 5 V.** It's a 3.3 V part.

The Fermion microSD module accepts 3.3–5 V; feed it 3.3 V so its logic levels
match the ESP32 directly.

## Signal nets

| From | To | Notes |
|---|---|---|
| GPIO 18 | RC522 SCK | |
| GPIO 19 | RC522 MISO | |
| GPIO 23 | RC522 MOSI | |
| GPIO 5 | RC522 SDA (CS) | |
| GPIO 22 | RC522 RST | |
| — | RC522 IRQ | not connected |
| GPIO 14 | microSD SCK | |
| GPIO 27 | microSD MISO | |
| GPIO 13 | microSD MOSI | |
| GPIO 4 | microSD CS | |
| GPIO 26 | MAX98357A BCLK | |
| GPIO 25 | MAX98357A LRC | |
| GPIO 33 | MAX98357A DIN | |
| GPIO 21 | MAX98357A SD | low = shutdown |
| — | MAX98357A GAIN | floating = 9 dB; 100 kΩ to VIN = 3 dB |
| GPIO 32 | 74AHCT125 1A (pin 2) | |
| 74AHCT125 1Y (pin 3) | 330 Ω → strip DIN | resistor close to the first LED |
| GND | 74AHCT125 1OE (pin 1) | OE is active low |
| GND | 74AHCT125 2A, 3A, 4A (pins 5, 9, 12) | tie unused inputs |
| — | 74AHCT125 2OE, 3OE, 4OE (pins 4, 10, 13) | tie to +5V (outputs disabled) |
| GPIO 16 | lamp button → GND | internal pull-up |
| GPIO 17 | encoder SW → GND | internal pull-up |
| GPIO 34 | encoder A | 10 kΩ to +3V3, 10 nF to GND |
| GPIO 35 | encoder B | 10 kΩ to +3V3, 10 nF to GND |
| GND | encoder C (common) | |

## Speaker

The speaker set ships with a 4-pin JST-PH plug. Cut it off one speaker and
crimp/solder the 2-pin JST-PH cable that fits the amplifier's speaker port.
The MAX98357A output is **bridge-tied**: neither speaker wire is ground. Never
connect either output to GND or to a scope ground clip.
