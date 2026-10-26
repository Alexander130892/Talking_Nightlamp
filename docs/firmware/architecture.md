# Firmware architecture

ESP-IDF v5.3, C, FreeRTOS. Rationale: [ADR 0011](../decisions/0011-firmware-framework.md).

## Layering

```
main/            app.c: wires everything together, owns the event queue
  │
  ├── logic (pure C, no ESP-IDF headers, unit-tested on PC)
  │     card_fsm      card presence → play / pause / resume / switch decisions
  │     tag_format    parse and build the 5-byte card record
  │     lamp_model    brightness 0..1 → 12 GRBW pixels (gamma + LED-count dimming)
  │     power_limit   scale a frame to a current budget
  │
  └── drivers (ESP-IDF, hardware-specific)
        rc522         SPI3: WUPA / SELECT / HLTA / read block or page
        storage       SDSPI on SPI2, FATFS mount, story folder lookup
        audio         MP3 decode → I²S → MAX98357A, amp shutdown pin
        led_strip     RMT driver for SK6812 GRBW
        input         lamp button, encoder (debounce, steps)
```

Logic modules know nothing about hardware. Drivers know nothing about policy.
`app.c` turns driver events into logic inputs and logic outputs into driver
calls.

## Tasks

| Task | Priority | Period | Job |
|---|---|---|---|
| `audio` | highest | streaming | Read SD, decode, write I²S DMA buffers |
| `rfid` | medium | 150 ms | Poll presence, post `CARD_SEEN(uid, record)` / `CARD_NONE` |
| `input` | medium | 5 ms | Debounce button, decode encoder, post events |
| `app` | medium | event-driven + 20 ms tick | Run `card_fsm`, drive lamp and audio |
| `lamp` | low | 20 ms (50 fps) | Animate, apply `lamp_model` + `power_limit`, push frame to RMT |

Audio gets the highest priority so RFID polling and SD access never starve the
I²S buffers.

## Card state machine (`card_fsm`)

Inputs: one poll result per period (`NONE`, or `CARD` with UID + parsed
record) and the current time in ms. Outputs: actions for the app
(`PLAY(story)`, `PAUSE`, `RESUME`, `STOP`, `LAMP_SIGNAL_LOST`,
`LAMP_RESTORE`, `UNKNOWN_CARD`).

```
            card stable (2 hits)
   IDLE ───────────────────────────► PLAYING
    ▲                                  │ card lost
    │                                  ▼
    │                                GRACE  (audio continues, lamp signals)
    │          same UID back ◄─────────┤
    │          within 3 s              │ 3 s expired
    │                                  ▼
    │   timeout (10 min)             PAUSED  (position kept in RAM)
    └──────────────────────────────────┤
                                       │ same UID → RESUME
                                       │ other valid card → PLAY(new)

  From PLAYING, GRACE or PAUSED: a different valid card → PLAY(new) immediately.
  From any state: story finished → STOP, lamp fades to glow then off → IDLE.
  Invalid / unknown tag, or two cards at once, in any state → UNKNOWN_CARD,
  state unchanged (grace timer keeps running).
```

Parameters live in one config struct (`present_hits`, `grace_ms`,
`pause_timeout_ms`) so tests and firmware share them.

## Boot sequence

1. Amp shutdown pin low (no pop).
2. Mount SD. On failure: lamp blinks a fault pattern, no audio.
3. Init RC522 (check version register: 0x91 or 0x92 for genuine, clones vary).
4. Init RMT, lamp off.
5. Enable amp, start tasks.

Target: ready in under 2 s.
