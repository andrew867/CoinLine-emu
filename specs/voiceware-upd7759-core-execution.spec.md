# Voiceware — uPD7759 core execution integration (emulator)

## Document control

| Field | Value |
| ----- | ----- |
| Status | **Implemented (partial)** — core path, filter tap, traces; datasheet polish TBD |
| Replaces as SoT | Software `decode_phrase` path in `millennium_voiceware_device` when feature flag **on** |
| Hardware spec | [`specs/hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md`](hardware/HW-UPD7759-VOICE-ROM-EXECUTION.spec.md) |

## Objective

Run **real `upd7759_device` playback** for terminal voice prompts such that:

- ROM bytes seen by the chip match hardware (banking via `set_rom_bank`).
- **`playing()`**, **`0x61` reads**, and **INT0** reflect **`busy_r()`** and datasheet rules (once known).
- **ROM images are immutable** at runtime for forensic reproducibility.
- Optional **IIR/FIR + gain** stage feeds the existing **speaker / audio route** path toward handset switching discovered in host firmware behavior.

## Architecture (target)

```
Z180 I/O (0x40/0x42/0x61)
        │
        ▼
millennium_voiceware_device  ──►  phrase/index adapter (read-only ROM directory) ──►  upd7759_device
        │                                                              │
        │◄──────────────────────── busy_r() / completion callbacks ──┘
        │
        ▼
post_filter (optional) ──► sound_stream / speaker ──► millennium_audio_route_device (handset path)
```

**Non-goals for v1:** Perfect analog match without captures; duplicating full VSYN RTOS in emulator.

## Feature flag

- **`COINLINE_VOICEWARE_UPD7759_CORE`** (env or INI): **default: on** (core path). Set to **`0`**, **`off`**, **`false`**, or **`no`** (case-insensitive) to force the legacy path (ROM header patch + software `decode_phrase` as primary).
- **CI** may still set `=1` explicitly for clarity (see [`test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md`](../test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md)).

## Phrase → chip feed

Today `write_phrase` expands phrases via **software decoder**. Target behavior:

1. Resolve **phrase index** + **bank** to a **ROM start offset** using the **same directory rules** as today’s `decode_phrase` **lookup only** (no PCM generation).
2. If MAME uPD7759 expects a **specific start protocol** (e.g. reset pulse + port write), implement **exactly** as MAME device API provides.
3. If **format mismatch** is detected at runtime, emit **`voice_fault`** trace and fall back (optional) to legacy decoder **only** when flag `COINLINE_VOICEWARE_LEGACY_FALLBACK=1`.

## Busy, status port `0x61`, IRQ

NEC **IC-2323A** pin/timing summary: `UPD7759-datasheet-extract.md`.

| Signal | Source of truth (target) |
| ------ | ------------------------- |
| Internal “playing” | `!upd7759_device::busy_r()` semantics per MAME + datasheet cross-check |
| `0x61` read | **Not a µPD7759 pin** — terminal CPU port mirror. Defaults `0xFF` / `0x7F` / `0x7F` (fault while voice reset asserted); override with `COINLINE_VOICEWARE_0x61_*` from **LA on the Z180 bus/glue**. NEC IC-2323A documents **I0–I7** (inputs) and **BUSY** (output), not `0x61` byte values. |
| INT0 | **No IRQ on chip** — host **INT0** derived from **BUSY** (or board logic). Assert on **busy → idle** edge per firmware; see **t_SSO** in datasheet for audio-path delay after **BUSY** release. |

Remove fixed **100 µs** timer polling as the **sole** completion mechanism; use **sound stream** or **device execute** hook to sample busy at emulation time granularity.

## ROM forensic integrity

- **Remove** or **disable** `patch_voicew_rom_headers_for_upd7759_master` when core path enabled.
- CI / evidence: record **SHA-256** of `voice_a.bin`, `voice_b.bin` in run metadata (existing voiceware ROM report pattern).

## Analog reconstruction (phase 2)

- Configurable **low-pass** (cutoff Hz, order) + **gain dB** in board profile JSON slice under `alerter` or new `voiceware_analog` key.
- Tap **after** uPD PCM, **before** route device, so handset switching sees filtered audio.
- Validate with WAV energy / spectrum compare vs hardware capture (test plan).

## Trace schema additions

Extend phrase detail / voiceware JSONL with:

- `execution_path`: `"legacy_software_decoder"` | `"upd7759_core"`
- `upd_busy_before`, `upd_busy_after` (bool)
- `rom_sha256_u16`, `rom_sha256_u26` (once per run or on change)

Startup JSON (`COINLINE_VOICEWARE_STARTUP_JSON`) includes `phrase_0x61_idle` / `busy` / `fault` and `retrigger_policy` for CI and bench reproducibility.

## Retrigger

- **Datasheet (IC-2323A):** “**MD, ST and WR are invalid while BUSY is low.**” A new **ST** while **BUSY** is **low** (active busy) is **not valid** on silicon.
- **`COINLINE_VOICEWARE_RETRIGGER_POLICY=suppress`** (default): duplicate `0x61` write while playback is active is ignored (trace: `voice_segment_retrigger_ignored`) — **aligned with datasheet** invalid-ST-during-BUSY.
- **`allow`** / **`always`** / **`always_strobe`**: always run the phrase-start path in emulation — **bench / diagnostic only**; not guaranteed by IC-2323A.

## Related documents

- [`voiceware-mame-device-implementation.spec.md`](voiceware-mame-device-implementation.spec.md) — update state machine section when core path lands
- [`test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md`](../test-plans/hardware/HW-UPD7759-VOICE-parity-tests.md)
