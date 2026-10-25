# 0009. Volume limited by speaker impedance and amplifier gain

Date: 2026-10-25
Status: Accepted

## Context

A child's hearing must be protected regardless of firmware bugs. A firmware
cap alone is one bug away from full volume.

## Decision

Limit volume in three layers:

1. **8 Ω speaker instead of 4 Ω.** Output power ≈ V²/R, so at 5 V the
   ceiling roughly halves (the amplifier module is rated 1.8 W into 8 Ω vs
   2.5 W into 4 Ω). Also halves peak current.
2. **Amplifier gain** via the MAX98357A GAIN pin. Start at the default (9 dB,
   pin floating) and lower it if the SPL test requires: 100 kΩ from GAIN to
   VDD gives 3 dB, the lowest setting.
3. **Firmware maximum** on the encoder volume.

The final setting is chosen by measurement: phone SPL meter at ~30 cm, kept at
quiet-conversation level. See the test plan.

## Consequences

- The firmware cap becomes a comfort setting, not the safety mechanism.
- Lower peak current also helps the power budget.
