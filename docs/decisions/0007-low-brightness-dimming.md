# 0007. Dim below one PWM step by switching LEDs off

Date: 2026-10-25
Status: Accepted

## Context

Each LED channel has 8-bit PWM (~1.2 kHz on the SK6812). At the bottom of the
range the steps are huge: 1 → 2 doubles the light, and 1 → 0 is on → off. A
slow fade to a night-light glow visibly steps, and 12 LEDs at value 1 may
still be too bright in a dark room.

| Mitigation | Gets below value 1? |
|---|---|
| Gamma correction | No, it remaps 0–255 but can't create sub-1 values |
| Warm-white channel | Looks better at value 1, same floor |
| Temporal dithering (0/1 alternating) | Badly: visible ~100 Hz flicker in a dark room |
| Thicker diffuser | Yes, shifts the whole range down optically |
| Fewer LEDs lit | Yes, 1 LED at value 1 is 1/12 of all 12 at value 1 |

## Decision

1. Gamma-corrected brightness for the normal range.
2. A thick diffuser so normal use sits at higher duty cycles.
3. Below a threshold, switch from "value per LED" to "number of LEDs lit":
   12 → 6 → 3 → 1 at value 1, then off.

## Consequences

- The brightness model is one function from a 0–1 brightness to a frame, which
  is testable on a PC.
- Low-level glow uses spatially spread LEDs; behind the diffuser this reads as
  one dim, even glow.
