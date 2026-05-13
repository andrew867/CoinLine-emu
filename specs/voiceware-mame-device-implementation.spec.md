# Voiceware — MAME device implementation specification

## Overview

Implement **`millennium_voiceware_device`** : `device_t` encapsulating voice playback command/status behavior described in [`docs/voiceware-emulation.md`](../docs/voiceware-emulation.md). Companion structs **`voiceware_command`**, **`voiceware_status`**, **`voiceware_trace_event`** are plain types used by trace emission.

**uPD7759 core execution:** See [`voiceware-upd7759-core-execution.spec.md`](voiceware-upd7759-core-execution.spec.md) and [`specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md`](hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md). When the core path is **active** (default: **on**; off only if `COINLINE_VOICEWARE_UPD7759_CORE` is explicitly falsey), phrase starts feed MAME **`upd7759_device`** (master `port_w` + start pulse); the voiceware **`sound_stream`** emits silence and **`playing()`** follows **`busy_r()`**. Software **`decode_phrase`** runs when the core path is **disabled**, or when core is on and **`COINLINE_VOICEWARE_LEGACY_FALLBACK=1`** after lookup failure.

## Class and files

| Item | Value |
| ---- | ----- |
| Class | `millennium_voiceware_device` |
| Headers | `src/mame/coinline/millennium_voiceware.h` |
| Implementation | `src/mame/coinline/millennium_voiceware.cpp` |
| Trace helpers | `src/mame/coinline/millennium_audio_trace.cpp` |

## Constructor dependencies

- **Owner**: `millennium_state` (or driver root)
- **Tags**: parent-provided tag string (e.g. `"voiceware"`)
- **Optional finder**: `millennium_audio_route_device &` for playback-active signaling (delegate methods)

## `device_t` lifecycle

| Phase | Behavior |
| ----- | -------- |
| `device_validity_check` | Assert required clock domain |
| `device_start` | Load timing constants from machine root config / INI |
| `device_reset` | Idle state; bank shadow cleared per profile; busy=false |

## Port bindings

Per [`fixtures/board/voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json) and [`docs/io-port-map.md`](../docs/io-port-map.md):

| Port | Direction | Role |
| ---- | --------- | ---- |
| `0x40` | Write | Bit **`0x08`**: voice reset (**active low**); pulse width ≥ `reset_pulse_min_usec` in map |
| `0x42` | Write | Bank merge `[3:0]` mask **`0x0F`** |
| `0x61` | Write | Phrase index / arm playback |

**Status / completion:** visibility may be **IRQ-only** or **polled status port** — IRQ path is **`compatibility_validation_required`** until [`fixtures/board/interrupt-map.json`](../fixtures/board/interrupt-map.json) wires `voice_playback_complete`. If no status port exists in map, implement completion via timer + IRQ trace only (document in compatibility ledger).

## Playback state machine (normative)

| State | Enter when |
| ----- | ---------- |
| `VW_IDLE` | Reset; fault cleared |
| `VW_RESET_ASSERTED` | `0x40` voice_reset bit driven inactive (device sees reset asserted per active-low rules) |
| `VW_BANK_LATCHED` | Valid `0x42` write while sequencing |
| `VW_ARMED` | Reset released + bank valid + awaiting phrase |
| `VW_PLAYING` | `0x61` written — software PCM draining **or** uPD core active (`!busy_r()`) when core flag on |
| `VW_COMPLETING` | Completion IRQ/timer pending — INT0 poll uses shared **`playing()`** / **`busy_r()`** when core flag on |
| `VW_FAULT` | Sequence violation or watchdog (`ic_watchdog_seconds` from map) |

## Command write behavior

1. Apply reset pulse semantics on `0x40` per map timing (`reset_pulse_min_usec`, `post_write_to_irq_min_usec`).
2. Merge bank on `0x42`.
3. Latch phrase on `0x61` and transition to **`VW_PLAYING`**.

Minimum spacing **≥ 10 µs** between phases where profile specifies (timer-based in emulator cycles).

## Status read behavior

Expose busy/ready/fault bits per profile — exact mask from [`voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json). Unknown bits → read-as-zero **with trace flag** `unknown_register_bits`.

## Prompt / sample ID handling

- **`prompt_id`** = `{bank, phrase_code}` tuple in traces.
- Invalid phrase → **`voice_fault`** trace event; optional IRQ if profile enables.

## Busy / ready timing

- **Legacy path:** nominal duration may be **profile-driven** (JSON sidecar `fixtures/audio/*.json` references duration_ms if present).
- **Core path:** duration follows **`upd7759_device`** emulation; **`playing()`** and **`0x61`** reads use **`busy_r()`** (MAME: **true** when idle). Placeholder read levels remain **`0xFF` / `0x7F`** until datasheet.

## Playback completion

Emit **`voice_segment_complete`** on **`0x61` read** when an idle transition is observed. **INT0** is asserted from the driver timer when **`playing()`** falls false — for the core path this reflects **`busy_r()`**, not PCM buffer exhaustion.

## Invalid command behavior

Sequence violating reset/bank/phrase rules → fault state; firmware must recover via reset pulse.

## Language / volume

If maps define language/volume ports or bits, bind explicitly; otherwise omit behavior and list under compatibility validation.

## Output routing signal

When playback active, call **`millennium_audio_route_device::notify_voice_active(bool)`** (exact API name TBD in implementation).

## Trace format

See [`audio-trace-format.spec.md`](audio-trace-format.spec.md). Minimum **`event_type`** values: `voice_reset_edge`, `voice_segment_start`, `voice_segment_complete`, `voice_fault`.

## Tests

| Type | Location |
| ---- | -------- |
| Fixture-only state machine | `tests/devices/test_voiceware_sequence.cpp` |
| MAME run | `voiceware-mame-implementation-tests.md` |

## Acceptance criteria

- [ ] All mapped ports handled without crashing unknown firmware builds.
- [ ] Traces contain start/complete/fault for scripted sequences.
- [ ] No scenario-only injection of busy/idle — **only** port writes move state.
- [ ] **Firmware-proof tests** invoke **`coinline-mame.exe`** (or `mamecoinline.exe`) with firmware and assert JSONL from that process — see [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md).
