# 0005. Story number and colour stored on the tag

Date: 2026-10-25
Status: Accepted

## Context

The box needs to know which story belongs to a card. Two places can hold that:

- **On the device**: a lookup file on the SD card mapping each tag's factory
  UID to a story folder (ESPuino approach).
- **On the tag**: a story number written into the tag's user memory
  (TonUINO approach).

Adding cards later is a nice-to-have. The person giving a new card will
usually not be near the box.

## Decision

Write a small, self-identifying record onto the tag:

```
'S' 'B' version story_id hue
```

The SD card only holds numbered story folders. Spare stories are pre-loaded
from day one, so a follow-up card works the moment it is placed.
Full format: [card-format.md](../firmware/card-format.md).

Tags are **not locked**: nobody rewrites a toddler's card by accident, and a
locked tag cannot be fixed. A spare written copy of each card ships with the
box instead.

## Alternatives considered

UID lookup on the device: blank cards work, but every new card needs its UID
added to the SD card beforehand, and a lost card means editing the mapping.

## Consequences

- A follow-up card is a blank tag plus one write. No action from the parents.
- Cards survive reflashing and SD replacement, and work in a second box.
- The magic bytes reject everything else a child puts on the lid, including
  contactless bank cards and phones.
- The firmware supports both MIFARE Classic 1K (block 4, default key) and
  NTAG21x (page 4), selected on the SAK value.
