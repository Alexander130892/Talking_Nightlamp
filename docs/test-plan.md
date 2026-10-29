# Test plan

Record results in the "Result" column with date and measured value. A block
passes only when every row in it passes.

## A. Devkit and power

| # | Test | Expected | Result |
|---|---|---|---|
| A0a | Blink GPIO 2, 500 ms (bringup/01_blink) | LED blinks, log every 500 ms | [x] 2026-10-25 |
| A0b | Boot log: chip and flash | Note revision and flash size | ESP32 rev v3.1, 4 MB flash [x] 2026-10-25 |
| A1 | Regulator marking | AMS1117-3.3 | `1117 33` [x] 2026-10-25 |
| A2 | VIN with USB power | 4.6–4.8 V (diode present) | 4.8 V [x] 2026-10-25 |
| A3 | Diode test, near USB port | ~0.2 V fwd, open rev | 0.2 V / ~2 V (meter limit) [x] 2026-10-25 |
| A4 | USB-C breakout + CC resistors, C-to-C charger | 5.0–5.2 V on VBUS | |
| A5 | 3V3 rail under full load (LEDs at budget + loud audio) | ≥ 3.25 V, no brown-out reset in 10 min | |
| A6 | Supply current, idle / playing / worst case | ~0.15 A / ~0.4 A / ≤ 0.8 A | |


## B. RFID

| # | Test | Expected | Result |
|---|---|---|---|
| B0 | Write/read-back ModWidthReg (0x24) | 0x55 reads back | 0x55 [x] 2026-10-26 |

| B1 | Read VersionReg (0x37) | 0x91 / 0x92 (clones vary, non-zero, not 0xFF) | [x]2026-10-26|
| B2 | Card type of bundled cards | SAK 0x08 = MIFARE Classic 1K | |
| B3 | Card left on reader 60 s | Reported present every poll | [x]2026-10-26|
| B3a | WUPA only, card left on reader | Alternates card / -- (READY → IDLE) | Alternating [x] 2026-10-26 |
| B3b | WUPA + retry, card left on reader | Card every poll, 220 ms period | [x] 2026-10-26 |
| B3c | Full cycle WUPA→SELECT→HLTA, card left on reader | UID every poll, 200 ms period | A2 E6 49 06, SAK 08 [x] 2026-10-26 |
| B4 | Card removed | Absent within 1 poll, grace starts | |
| B5 | Read range through the printed lid | ≥ 15 mm | |
| B6 | Contactless bank card on lid | Unknown card, state unchanged | |
| B7 | Two cards on reader | Collision every poll (HLTA also after failed select) | Collision every poll, 200 ms [x] 2026-10-26 |
| B8  | Auth key A + read block 4, CRYPTO1_OFF=1, two cards in turn | 16 bytes every poll, no deaf reader | A2 E6 49 06 + 55 42 6E 05: 16 × 00, ERR_MAGIC ✔ 2026-10-26 |
| B8a | Same with CRYPTO1_OFF=0 | 1 read, then deaf for all cards until reset | ✔ 2026-10-26 |
| B9  | Write record to block 4 (BOOT held), verify | WRITTEN + VERIFIED, 53 42 01 07 46 00… | A2 E6 49 06: story 7, hue 70 [x] 2026-10-26 |
| B9a | No BOOT / BOOT held after write / 2nd card | No write / no 2nd write / untouched | [x] 2026-10-26|
| B9b | Card removed and replaced | Record persists | [x] 2026-10-26|

## C. Storage and audio

| # | Test | Expected | Result |
|---|---|---|---|
| C0 | microSD module mod: R33–R35 bridged | SCK/SS/MOSI → GND ≈ 2.2 kΩ, no shorts | As predicted [x] 2026-10-29 |
| C1 | Mount FAT32 card, list /stories | Folders listed | [x] 2026-10-29 (card was exFAT: reformatted FAT32/MBR) |
| C1a | 256 KiB write + read-back verify at 10 MHz SPI | PASS | write 449 kB/s, read 431 kB/s (~36× MP3 need) [x] 2026-10-29 |
| C2 | 440 Hz / 1 kHz test tones at -20 dBFS (bringup/08_tone) | Clean, no clicks | [x] 2026-10-29 |
| C3 | Power-on with amp (shutdown → I2S silence → enable) | No audible pop | clean [x] |
| C4 | MP3 playback 30 min | No dropouts | |
| C5 | Playback while RFID polling + LED animating | No dropouts | |
| C6 | **Max volume at 30 cm (phone SPL meter)** | Quiet-conversation level; record dB value and GAIN setting | |

## D. Lamp

| # | Test | Expected | Result |
|---|---|---|---|
| D1 | All 12 LEDs warm white | Even colour | |
| D2 | Fade 100 % → glow → off over 30 s in a dark room | No visible steps or flicker | |
| D3 | Current limiter: request full RGBW white | Measured LED current ≤ 150 mA | |
| D4 | Phone camera on the lamp | Note banding (informational) | |

## E. Behaviour (requirements F1–F12)

| # | Test | Expected | Result |
|---|---|---|---|
| E1 | Place card | Lamp warms, story starts (F1) | |
| E2 | Lift card 1 s, put back | No interruption, lamp signal only (F2, F3) | |
| E3 | Lift card 5 s | Pauses after 3 s (F4) | |
| E4 | Put same card back | Resumes (F5) | |
| E5 | Pause, wait timeout | Next placement restarts story (F6) | |
| E6 | Swap card during grace | New story immediately (F7) | |
| E7 | Story ends | Fade to glow, then off (F8) | |
| E8 | Unknown tag | Pulse + chime, no state change (F9) | |
| E9 | Lamp button | Toggles lamp at any time (F10) | |
| E10 | Encoder | Volume steps, stops at max (F11) | |
| E11 | Newly written card, folder pre-loaded | Plays (F12) | |
| E12 | Pull the plug mid-story, 20 times | Boots normally every time, SD intact (N4) | |
| E13 | Boot time | < 2 s to ready (N4) | |
