# Alerter audio — MAME implementation test plan

## Purpose

Validate extended **`millennium_audio_device`**: tone requests, cadence, trace fallback vs speaker output.

## Harness contract (normative)

| Class | Firmware proof requires **`coinline-mame.exe`** run |
| ----- | --------------------------------------------------- |
| **A** | GoogleTest — traces only, no firmware claim |
| **C** | Emulator + firmware — assert `alerter_*` events from JSONL |

Speaker/listener verification in a browser is **not** a conformance gate.

## Test taxonomy

| Class | Notes |
| ----- | ----- |
| **A** | Parse fixtures `alerter-error-beep.json`, `alerter-service-beep.json` — compare trace intervals |
| **C** | Firmware-driven beeps during fault/service flows |

## Fixtures

- `fixtures/audio/alerter-error-beep.json`
- `fixtures/audio/alerter-service-beep.json`

## Scenario

- `fixtures/scenarios/alerter-output.json`

## Expected traces

`alerter-trace.jsonl`: `alerter_tone_start`, `alerter_tone_end`.

## Pass criteria

- Cadence edges within ±1 master tick of fixture (document slack in assertion).
- `-sound none` still yields trace proof.

## Fail criteria

- Tone state flipped without corresponding port write sequence.

## Source files touched

`millennium_audio.cpp/.h`, optional layout artwork hooks only as indicators — traces remain proof.

## Artifact outputs

`audio/alerter-trace.jsonl`, optional `audio/alerter.wav`.
