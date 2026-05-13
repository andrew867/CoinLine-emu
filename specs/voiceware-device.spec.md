# Spec — Voice playback device (voice prompts)

## Device name

**Voice playback device** (stored-phrase / tone playback IC).

## MAME class / file target

- `millennium_voiceware.cpp` / `millennium_voiceware.h` (to be added adjacent to `millennium_vfd.cpp`)

## Registers / ports

Pinned from integration constants (see `fixtures/board/voiceware-command-map.json`):

| Port | Direction | Purpose |
| ---- | --------- | ------- |
| `0x40` | W | Hardware control image — voice reset bit `0x08` |
| `0x42` | W | PIO port B — ROM bank select `[3:0]` |
| `0x61` | W | Phrase code write |

**Status:** Board decode collisions must be resolved per profile (`compatibility_validation_required` if ambiguous).

## Command enum (logical)

Combined **bank nibble + phrase byte** per segment. Segment metadata (last segment, repeat, special numeric expansion) is carried in the emulator’s phrase composer — **`compatibility_validation_required`** to match terminal catalog unless golden ROM-side tables are loaded.

## Status enum (logical)

| State | Meaning |
| ----- | ------- |
| `idle` | Reset asserted / no playback |
| `armed` | Completion IRQ armed |
| `playing` | Between start write and completion |
| `fault` | Watchdog elapsed without completion |

## Sample / prompt ID model

- **Logical index** → ordered list of `{bank_bits, code_byte}` segments (catalog per language profile).
- Special macros compose currency/minutes segments without expanding ROM externally.

## Busy / ready timing

- Minimum reset pulse width **18 µs** (integration comment).
- Minimum delay **10 µs** between phrase write and IRQ enable (integration comment).
- Inter-segment delay default **100 ms** (`10` ticks × 10 ms).
- IC watchdog **60 s** (alarm timer).

## Playback event model

| Event | Payload |
| ----- | ------- |
| `voice_reset` | `{level}` |
| `voice_bank` | `{mask}` |
| `voice_code` | `{byte}` |
| `voice_complete` | `{}` |
| `voice_timeout` | `{}` |

## Audio output model

Best-effort PCM optional; **`voice_segment_complete`** mandatory for correctness.

## Tests

[`../test-plans/voiceware-tests.md`](../test-plans/voiceware-tests.md)

## Trace output

JSON lines via device trace sink (`device=voiceware`).

## Acceptance criteria

1. Reset/bank/code ordering matches golden I/O capture.
2. Completion IRQ arrives within modeled duration unless test disables IRQ.
3. Watchdog raises `voiceware_fault` trace when IRQ suppressed.
