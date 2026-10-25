# Pin mapping

Board: ESP32-WROOM-32, 30-pin devkit, USB-C. The single source of truth in
code is [`firmware/main/board.h`](../../firmware/main/board.h); keep this
table in sync with it.

## Assignments

| Function | Signal | GPIO | Notes |
|---|---|---|---|
| RC522 (SPI3 / VSPI) | SCK | 18 | |
| | MISO | 19 | |
| | MOSI | 23 | |
| | SDA / CS | 5 | Strapping pin; CS idles high, which is the safe boot state |
| | RST | 22 | |
| microSD (SPI2 / HSPI, via GPIO matrix) | SCK | 14 | |
| | MISO | 27 | **Not GPIO 12**: an SD card pulls MISO high, and GPIO 12 high at boot selects 1.8 V flash |
| | MOSI | 13 | |
| | CS | 4 | |
| MAX98357A (I²S0) | BCLK | 26 | |
| | LRC (WS) | 25 | |
| | DIN | 33 | |
| | SD (shutdown) | 21 | Low = amp off (mute during boot, kills pop). High = on, left channel (firmware sends mono on both channels) |
| SK6812 strip (RMT) | DATA | 32 | Through 74AHCT125 → 330 Ω → strip DIN |
| Lamp button | IN | 16 | Internal pull-up, active low |
| Encoder | SW | 17 | Internal pull-up, active low |
| | A | 34 | Input-only, **external 10 kΩ pull-up** to 3.3 V + 10 nF to GND |
| | B | 35 | Input-only, **external 10 kΩ pull-up** to 3.3 V + 10 nF to GND |
| UART0 console | TX / RX | 1 / 3 | Via the devkit USB-serial chip |

Spare: GPIO 2 (strapping, on-board LED), 15 (strapping), 36, 39 (input-only).

## Why two SPI buses

The SD card streams audio continuously while the RC522 is polled every
100–200 ms. Separate buses keep RFID polling from stalling the audio stream,
and avoid microSD modules that don't release MISO from interfering with the
RC522.

## Constraints respected

- GPIO 6–11: flash, never used.
- GPIO 34–39: input-only, no internal pull-ups or pull-downs.
- Strapping pins at boot: GPIO 0 (not used), 2 (spare), 5 (high, CS idle),
  12 (not used), 15 (spare).
