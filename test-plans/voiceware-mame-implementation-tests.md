# Voiceware — MAME implementation test plan

## Purpose

Validate **`millennium_voiceware_device`**: port decode, state machine, timing, traces, and (when applicable) real firmware I/O. **Distinguish** unit/fixture tests from MAME firmware runs.

## Harness contract (normative)

| Class | Executable involved | May claim “firmware exercised voiceware”? |
| ----- | ------------------- | ---------------------------------------- |
| **A** | Host GoogleTest only (`ctest`, no `coinline-mame.exe`) | **No** — device fixture only |
| **B** | `coinline-mame.exe` boots but test asserts only reset/instantiation | **No** |
| **C** | **`coinline-mame.exe`** / **`mamecoinline.exe`** with **`flash.bin`**, stdout/stderr logs show emulator argv | **Yes** — assert `voiceware-trace.jsonl` from **that** process |

Browser-based or Web Audio validation is **out of scope** for conformance (see [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md)).

## Test taxonomy

| Class | Firmware | Proves |
| ----- | -------- | ------ |
| **A — Fixture unit** | No | State transitions from synthetic `write()` calls |
| **B — MAME CPU, no behavior claim** | Optional | Device instantiates, reset works |
| **C — MAME + real firmware** | **Required** | Phrase / reset / status patterns in traces |

**Rule:** Class C tests must not pass when firmware is absent except by **explicit skip** with message.

## Prerequisites

- Windows or MSYS2 MINGW64 per [`docs/BUILDING.md`](../docs/BUILDING.md)
- `coinline-emu` build; `build/bin/coinline-mame.exe` (or `mamecoinline.exe` per project)
- **Class C:** `../firmware/flash.bin` path and hash in `fixtures/firmware/firmware-hashes.json`

## Fixtures

| Fixture | Use |
| ------- | --- |
| `fixtures/audio/voiceware-command-reset.json` | Reset pulse + bank + phrase |
| `fixtures/audio/voiceware-play-prompt.json` | Full play sequence |
| `fixtures/audio/voiceware-invalid-prompt.json` | Fault path |

## Scenarios (external inputs only)

- `fixtures/scenarios/voice-prompt-playback.json` — may apply hook / key **inputs**; must not set internal voiceware state in JSON.

## Commands

**Build**

```text
tools/windows/build-mame-coinline.ps1
```

**Class A (GoogleTest / CTest when implemented)**

```text
ctest --test-dir build -R voiceware
```

**Class C run**

```text
tools/windows/run-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin -RunSeconds 120
```

**Class C validate (when tools exist)**

```text
tools/windows/test-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin
```

## Expected trace files

| Artifact | Location |
| -------- | -------- |
| `voiceware-trace.jsonl` | `build/runs/<stamp>/` |
| `audio-trace.jsonl` | same |

## Expected trace events (minimum)

- `voice_reset_edge` when reset bit toggles
- `voice_segment_start` with `prompt_id`
- `voice_segment_complete` OR `voice_fault` for invalid fixture path

## Pass criteria

- Class A: Fixture replay matches golden internal state dump.
- Class C: At least one **`voice_segment_*`** event appears **after** firmware-loaded milestone when voiceware ports are exercised by firmware.

## Fail criteria

- Internal busy flag toggled without port writes (scenario cheat).
- Class C passes without firmware binary (unless skipped).

## Source files touched (implementation)

`millennium_voiceware.cpp/.h`, `millennium_io.cpp/.h`, `millennium_audio_trace.cpp/.h`, optional `millennium.cpp/.h`

## Artifact outputs

Test logs, JSONL traces, optional evidence bundle directory per [`docs/evidence-bundles.md`](../docs/evidence-bundles.md).
