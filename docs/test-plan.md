# Test plan

Record results in the "Result" column with date and measured value. A block
passes only when every row in it passes.

## A. Devkit and power

| # | Test | Expected | Result |
|---|---|---|---|
| A0a | Blink GPIO 2, 500 ms (bringup/01_blink) | LED blinks, log every 500 ms | ✔ 2026-10-25 |
| A0b | Boot log: chip and flash | Note revision and flash size | ESP32 rev v3.1, 4 MB flash ✔ 2026-10-25 |
| A1 | Regulator marking | AMS1117-3.3 | `1117 33` ✔ 2026-10-25 |
| A2 | VIN with USB power | 4.6–4.8 V (diode present) | 4.8 V ✔ 2026-10-25 |
| A3 | Diode test, near USB port | ~0.2 V fwd, open rev | 0.2 V / ~2 V (meter limit) ✔ 2026-10-25 |
| A4 | USB-C breakout + CC resistors, C-to-C charger | 5.0–5.2 V on VBUS | |
| A5 | 3V3 rail under full load (LEDs at budget + loud audio) | ≥ 3.25 V, no brown-out reset in 10 min | |
| A6 | Supply current, idle / playing / worst case | ~0.15 A / ~0.4 A / ≤ 0.8 A | |


## B. RFID

| # | Test | Expected | Result |
|---|---|---|---|
| B1 | Read VersionReg (0x37) | 0x91 / 0x92 (clones vary, non-zero, not 0xFF) | |
| B2 | Card type of bundled cards | SAK 0x08 = MIFARE Classic 1K | |
| B3 | Card left on reader 60 s | Reported present every poll | |
| B4 | Card removed | Absent within 1 poll, grace starts | |
| B5 | Read range through the printed lid | ≥ 15 mm | |
| B6 | Contactless bank card on lid | Unknown card, state unchanged | |

## C. Storage and audio

| # | Test | Expected | Result |
|---|---|---|---|
| C1 | Mount FAT32 card, list `/stories` | Folders listed | |
| C2 | 1 kHz test tone | Clean tone, no clicks | |
| C3 | Power-on with amp | No audible pop | |
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
