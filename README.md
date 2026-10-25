# Talking Nightlamp

A bedtime-story box for toddlers that doubles as a night lamp.
Put a picture card on the lid: the lamp warms up and a story starts.
Lift the card: the story pauses. Story ends: the lamp fades to a dim glow,
then off.

Built around an ESP32, an RC522 RFID reader, an I²S amplifier and a ring of
SK6812 RGBW LEDs. Firmware is written from scratch in C on ESP-IDF.
Inspired by [TonUINO](https://www.voss.earth/tonuino/) and
[ESPuino](https://github.com/biologist79/ESPuino), but shares no code with
either.

<!-- Uncomment once images/final-build.jpg exists:
![Finished build](images/final-build.jpg)
-->
*Photo of the finished build follows in December 2026.*

## Features

- One card = one story. Cards carry their own story number and lamp colour.
- New cards can be added later without touching the box (write a card, mail it).
- Card off → audio keeps playing for a 3 s grace period while the lamp signals,
  then pauses. Card back → resumes.
- Warm-white night lamp (3000 K) with per-card colour accents and a
  sub-step fade to a very low glow.
- Volume limited in hardware (8 Ω speaker, amplifier gain) and in firmware.
- Mains-powered over USB-C, no battery, no WiFi, no cloud.
- Fully enclosed, screwed housing; no small detachable parts.

## Status

| Area | State |
|---|---|
| Requirements & design decisions | done |
| Parts ordered | done (Oct 2026) |
| Breadboard bring-up | not started |
| Firmware | skeleton only |
| Enclosure | not started |
| Final build | not started |

## Repository map

| Path | Contents |
|---|---|
| [`docs/requirements.md`](docs/requirements.md) | What the box must do, and the safety constraints |
| [`docs/decisions/`](docs/decisions/) | Architecture decision records: why each choice was made |
| [`docs/hardware/`](docs/hardware/) | Pin mapping, wiring, power budget, bill of materials |
| [`docs/firmware/`](docs/firmware/) | Firmware architecture, state machine, card data format |
| [`docs/content/`](docs/content/) | How to prepare the SD card and record stories |
| [`docs/build-guide.md`](docs/build-guide.md) | Step-by-step build instructions |
| [`docs/test-plan.md`](docs/test-plan.md) | Bring-up checks and acceptance tests |
| [`firmware/`](firmware/) | ESP-IDF project |
| [`test/host/`](test/host/) | Unit tests for hardware-independent logic, run on a PC |
| [`hardware/`](hardware/) | Schematic |
| [`mechanical/`](mechanical/) | 3D-printable enclosure (STL + source) |
| [`tools/`](tools/) | Helper scripts (card writing, audio conversion) |

## Quick start (firmware)

Requires [ESP-IDF v5.3](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32/get-started/).

```sh
cd firmware
idf.py set-target esp32
idf.py build
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

Host unit tests (no hardware needed):

```sh
make -C test/host test
```

## Building one yourself

1. Order the parts in the [bill of materials](docs/hardware/bom.md).
2. Follow the [build guide](docs/build-guide.md).
3. Prepare an SD card as described in [content](docs/content/audio-content.md).
4. Write cards (see [card format](docs/firmware/card-format.md)).

## License

- Firmware and tools: [MIT](LICENSE)
- Documentation, hardware and enclosure: [CC BY-SA 4.0](LICENSE-HARDWARE-DOCS.md)
