# Requirements

Target user: a 3-year-old child, in her bedroom. Secondary users: her parents.
Gift deadline: 25 December 2026.

## Functional

| ID | Requirement |
|---|---|
| F1 | Placing a story card on the lid starts that card's story and warms up the lamp. |
| F2 | Removing the card starts a 3 s grace period: audio keeps playing, the lamp signals the change. |
| F3 | If the same card returns within the grace period, nothing is interrupted. |
| F4 | After the grace period the story pauses and the playback position is kept in RAM. |
| F5 | The same card placed on a paused box resumes the story. |
| F6 | A paused story falls back to idle after a timeout (default 10 min) or a power cycle; the next placement starts from the beginning. |
| F7 | A different card replaces the current story immediately, also during the grace period. |
| F8 | When a story ends, the lamp fades to a dim glow, then off. |
| F9 | A card or object the box doesn't recognise gives one soft lamp pulse and a short chime, and does not change the current state. |
| F10 | One large button toggles the lamp independently of any story. |
| F11 | A rotary encoder sets the volume, limited by a firmware maximum. |
| F12 | New cards can be added later by writing a story number onto a blank tag, without changing the box (nice to have). |
| F13 | Two or more cards on the lid at once are treated as an unknown card (F9): one pulse + chime per placement. A playing story follows the normal grace rules: removing the extra card within 3 s continues it, otherwise it pauses. |
| F14 | Internal pull-up, active low; mute (F14)

## Non-functional

| ID | Requirement |
|---|---|
| N1 | Powered from a 5 V USB-C wall adapter. No battery. |
| N2 | Peak supply current ≤ 800 mA, so any adapter ≥ 1 A works. |
| N3 | No WiFi, no Bluetooth, no cloud dependency. Content lives on a microSD card. |
| N4 | Boot to ready in under 2 s. Pulling the plug at any moment must be harmless. |
| N5 | Volume ceiling enforced in hardware first, firmware second. |
| N6 | Lamp shows no visible flicker or stepping at low brightness. |

## Safety constraints

| ID | Constraint |
|---|---|
| S1 | Fully enclosed housing, closed with screws (no snap-fits a child can open). |
| S2 | No small detachable parts, no coin cells, no exposed electronics. |
| S3 | Cards are large enough not to be a choking hazard. |
| S4 | Mains adapter is a CE-marked product from a known brand; nothing mains-side is built by hand. |
| S5 | Supply input protected by a resettable fuse. |
| S6 | Power cable kept short and routed out of reach. |
| S7 | Maximum sound level at 30 cm measured and kept at quiet-conversation level. |
