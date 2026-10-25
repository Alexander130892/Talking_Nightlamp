# Firmware

ESP-IDF v5.3 project in C. Architecture: [docs/firmware/architecture.md](../docs/firmware/architecture.md).

## Build and flash

```sh
. $IDF_PATH/export.sh
idf.py set-target esp32      # once
idf.py build
idf.py -p <port> flash monitor
```

On macOS the port is typically `/dev/tty.usbserial-*` or `/dev/tty.wchusbserial-*`.

## Layout

| Path | Contents | State |
|---|---|---|
| `main/board.h` | All GPIO numbers | done |
| `main/app.c` | Boot sequence, task wiring | skeleton |
| `components/logic/` | Pure C, no ESP-IDF headers, unit-tested on a PC | `tag_format` done, rest stubbed |
| `components/rc522/` | MFRC522 over SPI3 | stub |
| `components/storage/` | SDSPI + FATFS on SPI2 | stub |
| `components/audio/` | MP3 → I²S → MAX98357A | stub |
| `components/lamp_strip/` | SK6812 RGBW over RMT | stub |
| `components/input/` | Button + encoder | stub |

Every stub returns `ESP_ERR_NOT_SUPPORTED`, so the boot log shows which
blocks still need work:

```
I (xxx) app: audio      ESP_ERR_NOT_SUPPORTED
...
```

## Working on a logic module

The tests in [`test/host/`](../test/host/) are the specification.

```sh
make -C ../test/host spec     # all suites, including unimplemented modules
make -C ../test/host test     # only implemented suites (what CI runs)
```

1. Read the contract in the module's header.
2. Run `make spec` and watch its suite fail.
3. Implement until the suite passes.
4. Move the suite from `SPEC_ONLY` to `SUITES` in `test/host/Makefile`,
   so CI guards it from then on.

`tag_format.c` is implemented as a style reference.

## Style

BARR-C:2018 inspired: fixed-width types, `p_` prefix for pointers, `b_`
for booleans, constants on the left in comparisons, one exit per function.
Formatting via `.clang-format` in the repo root.
