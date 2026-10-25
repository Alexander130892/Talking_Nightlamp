# Build guide

Status: draft. Steps marked 📷 get a photo in `images/` during the build.

Tools: soldering iron, multimeter, wire strippers, 3D printer, M3 heat-set
insert tip, a PC with ESP-IDF.

## 1. Check the devkit before wiring anything

Every ESP32 devkit is slightly different. Repeat these measurements on yours;
they decide whether the power design in
[ADR 0008](decisions/0008-power-architecture.md) applies unchanged.

1. Read the 3-pin regulator marking near the USB port. `1117 33` = AMS1117-3.3.
2. Power the board from USB. Measure **VIN → GND**:
   - ~4.6–4.8 V: a diode sits between USB and VIN (expected).
   - ~5.0–5.2 V: no diode. **Never** connect the laptop while the external
     supply is on.
3. Unpowered, diode mode across the 2-pin part near the USB port: ~0.2–0.4 V
   one way = Schottky.

## 2. Breadboard bring-up, one block at a time

Follow [test-plan.md](test-plan.md) section by section. Don't add the next
block until the current one passes.

1. ESP32 alone: flash the skeleton, see the boot log.
2. RC522: read the version register, then UIDs.
3. microSD: mount, list `/stories`.
4. MAX98357A + speaker: test tone, then an MP3.
5. 74AHCT125 + LED strip segment.
6. Button and encoder.

## 3. Prepare the LED segment

1. Cut 12 LEDs from the strip **on the copper cut marks** 📷.
2. Solder 5V / DIN / GND leads to the input end (arrow points away from it).
3. Wrap around the printed cylinder, LEDs facing outward.

## 4. USB-C power input

1. Solder 5.1 kΩ from **CC1 → GND** and **CC2 → GND** on the breakout 📷.
2. Check: a USB-C to USB-C charger now delivers 5 V at the VBUS pin.
3. Polyfuse in series with VBUS, then the 1000 µF star point.

## 5. Perfboard assembly

To do: perfboard layout drawing. Principles:

- Socket the 74AHCT125 (DIP-14 socket).
- Star ground: every module's GND wire returns to the star point.
- Short, thick wires from the star point to the amplifier.
- 330 Ω in the LED data line next to the strip input; 470 µF across the
  strip's 5V/GND.

## 6. Enclosure

To do: print settings and assembly order, see [mechanical/](../mechanical/).

- Body: opaque PLA/PETG. Dome: translucent white, ≥ 2 mm wall for diffusion.
- M3 heat-set inserts, screws only, no snap-fits.
- RC522 antenna directly under the card recess, ≤ 3 mm plastic between.

## 7. Content and cards

1. Prepare the SD card: [audio-content.md](content/audio-content.md).
2. Write cards: [card-format.md](firmware/card-format.md).

## 8. Final checks

Run the acceptance tests in [test-plan.md](test-plan.md), including the
volume measurement, before closing the box.
