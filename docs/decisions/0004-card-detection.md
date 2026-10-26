# 0004. Presence polling with WUPA and a 3 s grace period

Date: 2026-10-25
Status: Accepted

## Context

The common MFRC522 libraries expose "is a *new* card present". That call sends
REQA, which only wakes cards in the IDLE state. After a card has been read and
halted (HLTA) it stays in HALT for as long as it remains powered by the field,
and ignores REQA. So "no new card" says nothing about whether the card is
still on the reader. Removal is not reported at all.

A 3-year-old will also half-lift and shuffle cards, so single missed polls
must not toggle playback.

## Decision

Poll for presence every 100–200 ms:

1. Send **WUPA** (wakes cards in IDLE *and* HALT).
2. Anticollision + SELECT → read the UID, compare with the current card.
3. Send HLTA, **also when step 2 failed**. A card left in READY ignores the
   next WUPA and drops back to IDLE, so presence would alternate between
   "card" and "none". HLTA is an unexpected frame in READY: the card returns
   to IDLE silently and answers the next WUPA. Verified in bring-up 03/04
   (test plan B3a, B3c, B7).

Answer with the same UID = present. No answer = absent for this poll.
A different UID = card swap. Something answered but no single card could be
selected (two cards: collision) = unknown card, see requirement F13.

Debounce asymmetrically:

- **Present** after 2–3 consecutive hits (no playback from a half-placed card).
- **Absent** only after a 3 s **grace period**. During the grace period audio
  keeps playing and the lamp signals the change; the lamp is the user-facing
  buffer.

A different card during the grace period switches stories immediately.

## Alternatives considered

- Pause immediately on the first missed poll: flickers between pause and play
  on a shaky card.
- Keep the card ACTIVE and do a cheap page read each poll: works, but WUPA +
  SELECT handles HALT/IDLE uniformly and also detects swaps.

## Consequences

- The RFID driver needs WUPA/SELECT/HLTA primitives, not just the library's
  "new card" call.
- The card logic is a pure state machine fed by poll results and a clock, so
  it can be unit-tested on a PC, see `docs/firmware/architecture.md`.
