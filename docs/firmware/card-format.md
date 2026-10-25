# Card data format

Rationale: [ADR 0005](../decisions/0005-card-data-on-tag.md).

## Record (format version 1)

5 bytes, padded with `0x00` to the tag's block/page size.

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 1 | magic[0] | `0x53` (`'S'`) |
| 1 | 1 | magic[1] | `0x42` (`'B'`) |
| 2 | 1 | version | `0x01` |
| 3 | 1 | story_id | 1–255 → folder `/stories/NNN/` (0 is invalid) |
| 4 | 1 | hue | Lamp accent colour, 0–255 on the colour wheel |

A card is accepted only if the magic bytes and version match and the story
folder exists. Anything else is "unknown card" (soft pulse + chime, state
unchanged).

## Where the record lives

The tag type is selected on the SAK byte returned during SELECT.

| Tag | SAK | Location | Access |
|---|---|---|---|
| NTAG213/215/216 (Ultralight family) | `0x00` | Pages 4–5 (first user pages) | READ, no authentication |
| MIFARE Classic 1K | `0x08` | Block 4 (sector 1, block 0) | Authenticate with key A `FF FF FF FF FF FF` |

Classic sector 0 holds the manufacturer block and is never written.

## Writing a card

Planned: a serial console command in the firmware, so no phone app is needed:

```
write <story_id> <hue>
```

Alternative for NTAG stickers: any NFC app (e.g. NFC Tools) that can write raw
bytes to page 4: `53 42 01 <id> <hue>`.

Cards are not locked. Keep a spare written copy of each card.
