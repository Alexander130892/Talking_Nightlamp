# 0003. microSD only, no WiFi

Date: 2026-10-25
Status: Accepted

## Context

New stories could reach the box by copying files to the SD card, or by
fetching them over WiFi from a server.

## Decision

**microSD only. WiFi and Bluetooth stay off.**

## Consequences

- No credentials, no network setup for the parents, no cloud dependency, no
  privacy questions about a device in a child's bedroom.
- Removes WiFi TX current peaks (350–500 mA) from the power budget.
- New stories require physical access to the SD card. Mitigated by putting
  spare stories on the card from day one, see
  [0005](0005-card-data-on-tag.md).
- No over-the-air firmware updates. Updates go over USB with the housing
  opened.
