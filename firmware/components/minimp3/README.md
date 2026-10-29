# minimp3 (vendored)

MP3 decoder used by the audio pipeline, see
[ADR 0013](../../../docs/decisions/0013-mp3-decoder.md).

| | |
|---|---|
| Upstream | https://github.com/lieff/minimp3 |
| Commit | ea99364f61c14656440e8d77e9c233ccf3124633 |
| Licence | CC0 1.0 (public domain), see `LICENSE` |
| Files | `include/minimp3.h` unmodified; `minimp3.c` only instantiates it |

Notes for users of this component:

- `mp3dec_decode_frame()` puts a `mp3dec_scratch_t` (~16 KB) **on the stack**.
  Call it from a task with at least 24 KB of stack.
- `mp3dec_t` (~6.7 KB) should be static or heap-allocated, not on the stack.
- Built with `-O2` regardless of the project's optimisation level.
