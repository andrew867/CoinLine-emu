# Alerter audio

This document describes the alerter audio device, which generates call-progress tones, ringback, busy, dial tone, and DTMF digits as needed by the firmware.

## Purpose

Synthesize audible signals the firmware requests. Route audio to the simulated handset speaker and (optionally) to an output sink for evidence bundles.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Tone-control port | Selects tone (dial, ringback, busy, fast busy, off-hook warning). |
| DTMF port | Selects DTMF digit and duration. |
| Volume port | Per board profile. |

## State machine

```
silent -> tone_active -> silent
silent -> dtmf_active -> silent
```

## Timing

- Sample rate: per board profile (`alerter.sample_rate_hz`).
- Frequencies and cadences for each progress tone are pinned in the spec.

## Interrupts

Typically none. Some board profiles raise an interrupt when a DTMF digit completes.

## Fixtures

Alerter behavior is exercised via the scenario verb `expect_audio_event` and pinned by recorded WAV samples in evidence bundles. No standalone fixture file is required.

## Tests

- `tests/devices/test_alerter_dial_tone.cpp` — verifies dial-tone frequency.
- `tests/devices/test_alerter_ringback.cpp` — verifies ringback cadence.
- `tests/devices/test_alerter_busy.cpp` — verifies busy cadence.
- `tests/devices/test_alerter_dtmf.cpp` — verifies DTMF digit generation.

## Boot-milestone dependencies

- M5: first alerter I/O observed.
- Coin-call and card-call scenarios depend on M10.

## Acceptance criteria

- Each tone is generated at the spec'd frequency.
- DTMF digits match expected dual-tone frequencies (697/770/852/941 × 1209/1336/1477/1633).
- Audio output samples are exported in evidence bundles when requested.

## Cross-references

- [`../specs/alerter-audio-device.spec.md`](../specs/alerter-audio-device.spec.md).
- [`hookswitch-and-handset.md`](hookswitch-and-handset.md).
- Voice prompts (distinct path): [`voiceware-emulation.md`](voiceware-emulation.md), [`audio-routing-emulation.md`](audio-routing-emulation.md).
