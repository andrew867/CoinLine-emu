# Test plan — Voice playback device

## Proof strategy

Tests must demonstrate **ROM-driven** or **golden-trace-equivalent** behavior — static unit tests alone are insufficient.

| Proof tier | Requirement |
| ---------- | ----------- |
| **A — ROM execution** | Boot approved terminal ROM in emulator; reach a state that plays a prompt; **record port writes** to `0x40`/`0x42`/`0x61` with CPU cycle timestamps. |
| **B — Trace replay** | Replay a **hardware-validation I/O capture** (fixture) through the device model; outputs must match golden hash/order. |

Tier **A** or **B** is mandatory per case below.

## Preconditions

- Board profile binds `0x40` (`0x08`), `0x42`, `0x61`, completion IRQ.
- Interrupt controller routes voice-complete vector per profile.

## Cases

| ID | Name | Steps | Pass criteria |
| -- | ---- | ----- | ------------- |
| VW-01 | Reset/init | Tier A: reset vector runs until voice idle **or** Tier B: replay init trace | Shadow registers: reset asserted idle policy; bank=`0` |
| VW-02 | Play prompt | Tier A: exercise UI path that speaks fixed catalog prompt **or** Tier B: replay capture | Ordered writes: reset pulse → bank → code; `voice_segment_start` trace |
| VW-03 | IRQ completion | Tier A+B: after `0x61`, completion IRQ within modeled window **or** inject IRQ hook | `voice_segment_complete` logged; PC advances |
| VW-04 | IRQ watchdog | Suppress IRQ until watchdog | `voice_fault` / alarm trace |
| VW-05 | Timing spacing | Instrument — measure cycles between `0x61` write and IRQ arm | ≥ nominal 10 µs spacing when IRQ model enforces it |
| VW-06 | Language rebuild | Tier A: toggle language during prompt **or** Tier B: multi-language fixture | Second bank/code sequence differs per active table |
| VW-07 | RX conditioning coupling | Tier A: prompt while profile **`co_connected=true`** | Observed telephony opcode stream matches `RX_MUTE`→sidetone→`RX_UNMUTE` order (see audio routing tests) |
| VW-08 | Evidence bundle | Export trace + optional WAV | Trace mandatory; WAV optional |

## Fixtures

- `fixtures/board/voiceware-command-map.json`
- Hardware-validation golden trace (**`compatibility_validation_required`** until published path pinned)

## Negative tests

- Random `0x61` without reset sequence → deterministic **logged fault** (no silent success).
