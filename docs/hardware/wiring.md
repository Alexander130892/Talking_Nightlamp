# Wiring

A KiCad schematic will follow in [`hardware/schematic/`](../../hardware/schematic/).
Until then, this netlist is the reference. GPIO numbers follow
[pin-mapping.md](pin-mapping.md).

## Block diagram

```
USB-C breakout ──► polyfuse ──► STAR (+5V, 1000 µF) ─┬─► MAX98357A VIN ──► speaker 8 Ω
 (CC1, CC2 → 5.1 kΩ → GND)                          ├─► SK6812 strip 5V (+470 µF)
                                                     ├─► 74AHCT125 VCC (+100 nF)
                                                     ├─► microSD module VCC (own 3.3 V LDO)
                                                     └─► ESP32 devkit VIN ──► AMS1117 ──► 3V3
                                                                                └─► RC522 3.3V
GND: every module returns to the STAR point.
```

## Power nets

| Net | Connects |
|---|---|
| VBUS_IN | USB-C breakout VBUS → polyfuse pin 1 |
| +5V | polyfuse pin 2, 1000 µF (+), MAX98357A VIN, strip 5V, 470 µF (+) at strip, 74AHCT125 pin 14, microSD module +5, ESP32 VIN |
| +3V3 | ESP32 3V3 pin, RC522 3.3V, encoder pull-ups |
| GND | USB-C GND, both capacitors (−), all module GNDs, 74AHCT125 pin 7 |
| CC1 / CC2 | each through its own 5.1 kΩ to GND |

**Never connect the RC522 to 5 V.** It's a 3.3 V part.

**microSD module (DFRobot Fermion DFR0229): VCC on +5 V, and modified.**
It has its own BL8555-33 regulator (needs ~5 V in) and a 1 kΩ / 2.2 kΩ
divider on SCK, MOSI and SS, designed for 5 V logic. From a 3.3 V ESP32 the
card would see only ~2.27 V (V_IH 2.06 V). The three 1 kΩ series resistors
**R33, R34, R35 are bridged with solder**, so the card gets the full 3.3 V.
MISO is wired directly and needs nothing. After the mod the module is
**3.3 V logic only**: never connect it to a 5 V microcontroller.
See [ADR 0012](../decisions/0012-microsd-module-mod.md).

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
| GPIO 32 | 74AHCT125 1A (pin 2) | 5.1kOhm pulldown added |
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
## Connectors

Every link between the base and a removable part (lid, front panel) goes
through a JST-XH (2.54 mm) connector: the header is soldered on the
perfboard, the cable ends in a crimped housing. Each connector has a
different pin count, so no plug fits a wrong header, and XH housings only
fit one way round. Pin 1 is marked by the triangle on the housing and a
marker dot on the board.

Wire colours: **black is always GND, red is always the supply.** Only six
colours are available, so two wires carry a tape band at both ends.

### J1: lid, RC522 (7 pins)

| Pin | Signal | ESP32 / board | Colour |
|---|---|---|---|
| 1 | GND | GND | black |
| 2 | +3V3 | 3V3 | red, **white band** (this red is 3.3 V, never 5 V) |
| 3 | SCK | GPIO 18 | yellow |
| 4 | MOSI | GPIO 23 | green |
| 5 | MISO | GPIO 19 | blue |
| 6 | SDA (CS) | GPIO 5 | white |
| 7 | RST | GPIO 22 | yellow, **black band** |

### J2: lid, LED strip (3 pins)

| Pin | Signal | Board | Colour |
|---|---|---|---|
| 1 | GND | star GND | black |
| 2 | +5V | star +5V | red |
| 3 | DATA | 74AHCT125 1Y (pin 3) | green |

The 330 Ω data resistor and the 470 µF capacitor sit **in the lid**, at the
strip's input (resistor in series with DATA, capacitor across +5V/GND,
stripe to GND).

### J3: front panel, button and encoder (6 pins)

| Pin | Signal | ESP32 / board | Colour |
|---|---|---|---|
| 1 | GND | encoder C and one SW pin | black |
| 2 | ENC_A | GPIO 34 (10 kΩ pull-up + 10 nF on the perfboard) | yellow |
| 3 | ENC_B | GPIO 35 (10 kΩ pull-up + 10 nF on the perfboard) | green |
| 4 | ENC_SW | GPIO 17, other SW pin | blue |
| 5 | BTN | GPIO 16, button NO spade | white |
| 6 | GND | button COM spade | black |

### Not on XH connectors

| Link | How |
|---|---|
| Speaker → amplifier | 2-pin JST-PH (speaker set) |
| USB-C block → star point | soldered, 22–24 AWG red / black |

### Wiring practice

- 22–24 AWG for +5V and GND to the amp, LEDs and ESP32; 26–28 AWG for signals.
- Twist each bundle loosely; in J1 twist SCK around a GND wire.
- Lengths: J1 and J2 about 15 cm, J3 about 10 cm: the cube opens flat
  without unplugging.
- Every bundle is cable-tied to the base before it reaches a joint or plug.
