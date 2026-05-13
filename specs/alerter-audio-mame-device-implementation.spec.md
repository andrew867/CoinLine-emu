# Alerter / local audio output — MAME device implementation specification

## Overview

Extend **`millennium_audio_device`** (`millennium_audio.cpp` / `.h`, model `millennium_audio_model` as needed). Class name remains **`millennium_audio_device`** — no parallel class unless split is required for MAME registration.

## Responsibilities

| Concern | Approach |
| ------- | -------- |
| Buzzer / piezo / speaker path | Optional `speaker_device` stream + `mixer` |
| Cadence patterns | Table-driven: `service_beep`, `error_beep`, `user_prompt_tone` |
| Startup | On `device_reset` / first authorized I/O: emit trace **`alerter_ready`** |

## Port bindings (placeholder until map pins hardware)

[`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json) `alerter_path` currently has **`ports: []`**. Implementation requirement:

1. Emit **full** `alerter_tone_start` / `alerter_tone_end` / `alerter_gpio_write` traces for **every** decoded alerter-related I/O once ports exist.
2. Until ports are pinned, hook **unknown-port logger** correlation + compatibility row — **do not** silently drive speakers.

## MAME sound strategy

**Phase 1:** Trace-only cadence (`alerter-trace.jsonl`) — proves firmware toggled hardware-facing bits.

**Phase 2:** Bind `speaker_device` / wave generator to trace-driven edges.

## Trace events (minimum)

| `event_type` | When |
| -------------- | ---- |
| `alerter_ready` | Device initialized / reset complete |
| `alerter_gpio_write` | Raw port write affecting alerter path (when ports known) |
| `alerter_tone_start` | Cadence phase starts |
| `alerter_tone_end` | Cadence phase ends |

## Tests

[`test-plans/alerter-audio-mame-implementation-tests.md`](../test-plans/alerter-audio-mame-implementation-tests.md).

## Acceptance criteria

- [ ] Cadence matches fixture timing windows ±1 emulator tick (document slack in test plan).
- [ ] No scenario sets “alerter on” without port/bit activity.
- [ ] Firmware-proof tests run **`coinline-mame.exe`** per [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md).
