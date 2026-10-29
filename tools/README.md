# Tools

Tools:

| Tool | Purpose |
|---|---|
| `make_chime.py` | Generates `/system/unknown.mp3` (soft two-note bell for unknown cards), reproducibly, from scratch |
| `convert_audio.sh` (planned) | Batch-convert recordings to mono 96 kbit/s MP3 with normalised loudness |
| `make_sd_layout.py` (planned) | Create `/stories/NNN/` folders and check file names |

Card writing is done from the firmware's serial console (`write <id> <hue>`),
see [card format](../docs/firmware/card-format.md).
