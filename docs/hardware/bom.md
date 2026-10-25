# Bill of materials

Machine-readable version: [`bom.csv`](bom.csv). Prices as ordered from
[Tinytronics](https://www.tinytronics.nl) on 2026-10-25, incl. 21 % VAT.
"Used" is what one box needs; the rest are spares for prototyping.

## Electronics (ordered)

| # | Part | Used | Ordered | Price | Notes |
|---|---|---|---|---|---|
| 1 | [DFRobot MAX98357 I²S amplifier](https://www.tinytronics.nl/en/audio/amplifiers/dfrobot-max98357-i2s-amplifier-module-2.5w) | 1 | 2 | €17.00 | Spare: BTL outputs are easy to short while probing |
| 2 | [Speaker set 8 Ω 2 W, JST-PH](https://www.tinytronics.nl/en/audio/speakers/speakers/speaker-set-8%CF%89-2w-with-jst-ph-connector) | 1 | 1 set (2 pcs) | €3.00 | |
| 3 | [DFRobot Fermion microSD module](https://www.tinytronics.nl/en/data-storage/modules/dfrobot-fermion-microsd-card-module-for-arduino) | 1 | 2 | €8.50 | |
| 4 | [SK6812 RGBWW strip, warm white, 60 LED/m, 1 m](https://www.tinytronics.nl/en/lighting/led-strips/led-strips/sk6812-digital-5050-rgbww-led-strip-60-leds-1m-warm-white) | 12 LEDs (20 cm) | 1 m | €12.50 | |
| 5 | [SN74AHCT125N, DIP-14](https://www.tinytronics.nl/en/components/ics-and-microcontroller-chips/ics/sn74ahct125n-4-channel-level-converter-3.3v-to-5v-14-pin-dip) | 1 | 3 | €3.00 | |
| 6 | [14-pin IC socket](https://www.tinytronics.nl/en/components/sockets/14-pins-ic-socket) | 1 | 2 | €0.70 | |
| 7 | [USB-C female to DIP adapter](https://www.tinytronics.nl/en/cables-and-connectors/connectors/usb/usb-c-2.0-female-to-dip-adapter-including-headers) | 1 | 3 | €2.25 | Add 5.1 kΩ CC1/CC2 → GND |
| 8 | [Resistor 5.1 kΩ (10)](https://www.tinytronics.nl/en/components/resistors/resistors/resistor-5.1k%CF%89-1-4w-10-pieces) | 2 | 10 | €0.50 | USB-C CC pull-downs |
| 9 | [Polyfuse 1000 mA, through-hole](https://www.tinytronics.nl/en/components/fuses/self-restoring-fuse-pptc-polyfuse-1000ma-through-hole) | 1 | 3 | €3.60 | |
| 10 | [Electrolytic 1000 µF 25 V (5)](https://www.tinytronics.nl/en/components/capacitors/aishi-elektrolytic-capacitor-1000uf-25v-5-pieces) | 1 | 5 | €1.10 | Star point |
| 11 | [Electrolytic 470 µF 25 V (5)](https://www.tinytronics.nl/en/components/capacitors/aishi-elektrolytic-capacitor-470uf-25v-5-pieces) | 1 | 5 | €0.75 | At the LED strip |
| 12 | [Ceramic 100 nF 50 V](https://www.tinytronics.nl/en/components/capacitors/ceramic-capacitor-100nf-50v) | ~4 | 10 | €1.00 | Decoupling |
| 13 | [Ceramic 10 nF 50 V](https://www.tinytronics.nl/en/components/capacitors/ceramic-capacitor-10nf-50v) | 2 | 5 | €0.50 | Encoder A/B RC filter |
| 14 | [Resistor 100 kΩ (10)](https://www.tinytronics.nl/en/components/resistors/resistors/resistor-100k%CF%89-1-4w-10-pieces) | 0–1 | 10 | €0.50 | Amp GAIN, only if needed |
| 15 | [Resistor 330 Ω (10)](https://www.tinytronics.nl/en/components/resistors/resistors/resistor-330%CF%89-1-4w-10-pieces) | 1 | 10 | €0.50 | LED data series resistor |
| 16 | [Resistor 10 kΩ (10)](https://www.tinytronics.nl/en/components/resistors/resistors/resistor-10k%CF%89-1-4w-10-pieces) | 2 | 10 | €0.50 | Encoder pull-ups (GPIO 34/35) |
| 17 | [Large push button 24/40 mm, white](https://www.tinytronics.nl/en/switches/manual-switches/push-buttons-and-switches/large-white-push-button-24mm-of-40mm-reset) | 1 | 2 | €7.00 | Lamp button |
| 18 | [Rotary encoder EC11 with push](https://www.tinytronics.nl/en/switches/manual-switches/rotary-encoders/rotary-encoder-ec11-20mm) | 1 | 3 | €2.25 | Volume |
| 19 | [JST-PH 2p connector with cable](https://www.tinytronics.nl/en/cables-and-connectors/cables-and-adapters/jst-compatible/jst-ph-2p-compatible-connector-with-cable) | 1 | 2 | €0.80 | Speaker to amp |
| 20 | [Raspberry Pi USB-C power supply 5.1 V 3 A, white](https://www.tinytronics.nl/en/power/power-supplies/5v/raspberry-pi-usb-c-power-supply-5.1v-3a-white) | 1 | 1 | €11.00 | CE-marked, cable attached |
| | **Subtotal** | | | **€76.95** | + €8.82 PostNL to Belgium |

## Already owned

| Part | Notes |
|---|---|
| ESP32-WROOM-32 30-pin devkit, USB-C | AMS1117-3.3 regulator, Schottky USB→VIN |
| RC522 RFID module (13.56 MHz) | |
| MIFARE Classic 1K cards / tags | Bundled with the RC522 |
| microSD card ≤ 32 GB | FAT32 |
| Perfboard, pin headers, wire, heat-shrink | |
| M3 heat-set inserts + M3 screws | |
| PLA/PETG filament: opaque for the body, translucent white for the dome | |

## Optional

| Part | Why |
|---|---|
| NTAG213 stickers, 25 mm | Writable from any phone (also iPhone), no authentication needed |
