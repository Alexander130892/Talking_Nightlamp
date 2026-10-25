# 0010. Parts from one EU shop instead of AliExpress

Date: 2026-10-25
Status: Accepted

## Context

Prices compared for the complete parts list on 2026-10-25 (Belgium):

| Source | Total | Delivery |
|---|---|---|
| AliExpress (14 lines, ~10 sellers) | €71.53 in cart, **€127 at checkout** incl. shipping and import charges | 2–3 weeks |
| Tinytronics (NL), one parcel | ≈ €72 incl. €8.82 PostNL | 3–8 working days |
| Amazon.com.be | Not viable: multi-packs, no RGBW ring, no USB-C power breakout | — |

## Decision

Order everything from **Tinytronics**.

## Consequences

- Tinytronics has no 12-LED SK6812 RGBW ring, only RGB rings. The lamp uses a
  12-LED segment cut from a warm-white RGBW strip, wrapped around a printed
  cylinder. Arguably diffuses better than a flat ring.
- The USB-C breakout listing doesn't mention CC resistors; 5.1 kΩ resistors
  are added by hand.
- The only 8 Ω speaker is a small 2 W unit. Revisit if volume or quality is
  insufficient.
- All parts on hand within a week, so firmware bring-up starts early.
