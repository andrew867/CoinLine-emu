# Audio call-state integration — test plan

## Purpose

Prove cross-device consistency across **call states** defined in [`specs/audio-call-state-integration.spec.md`](../specs/audio-call-state-integration.spec.md). Evidence is **multi-trace correlation**, not VFD string matching alone.

## Harness contract (normative)

End-to-end proof **always** requires **`coinline-mame.exe`** with firmware and merged **`audio-trace.jsonl` + `io-trace.jsonl`**. Class A may replay stitched fixtures **without** claiming firmware.

## Prerequisites

Devices from tranches A1–A4 integrated; hookswitch, modem, voiceware attached.

## Test taxonomy

| Class | Scope |
| ----- | ----- |
| **A** | Scripted port replay producing expected **cross-field** JSON snapshot |
| **C** | End-to-end firmware run through scenario steps |

## Scenarios

Combine steps from:

- `fixtures/scenarios/audio-boot-init.json`
- `fixtures/scenarios/voice-prompt-playback.json`
- `fixtures/scenarios/disconnect-supervision.json`

## Commands

Full scenario runner (when implemented) + manual MAME run with trace env vars per [`docs/audio-boot-integration-plan.md`](../docs/audio-boot-integration-plan.md).

## Expected artifacts

- `audio-trace.jsonl` with populated `call_state_if_known` **only** when derivable from existing devices (optional field).
- `audio-state-final.json` summary in bundle.

## Pass criteria

- For each targeted state row in the spec, **at least one** test documents the trace combination used — no guessed hardware.

## Fail criteria

- Claiming firmware proved a state without Class C run.

## Source files touched

`millennium.cpp`, `millennium_state`, all audio devices, `millennium_modem.cpp`, `millennium_keypad.cpp`.

## Artifact outputs

Full evidence bundle per [`docs/audio-evidence-bundle-plan.md`](../docs/audio-evidence-bundle-plan.md).

## Implemented CMake targets

- Class A: `test_audio_call_state_fixture` — integration labels from `snapshot_integration_call_state` only.
- Class C: `test_audio_call_state_firmware` — skip **77** until **`coinline-mame.exe`** trace assertions are wired (see [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md)).
