# Spec — Alerter audio device

This spec defines the alerter audio contract. See [`../docs/alerter-audio.md`](../docs/alerter-audio.md) for context.

## Purpose

Synthesize call-progress tones, ringback, busy, dial tone, off-hook warning, and DTMF digits.

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Tone-control port | W | Selects tone. |
| DTMF port | W | Selects DTMF digit + duration. |
| Volume port | W | Per board profile. |

## I/O ports

Per `fixtures/board/io-port-map.json` rows with `device = "audio"`.

## State machine

```
silent -> tone_active -> silent
silent -> dtmf_active -> silent
```

## Timing behavior

- `sample_rate_hz` per board profile (8 kHz / 16 kHz typical).
- Tone frequencies and cadences pinned per spec.

## Tone reference

| Tone | Frequencies | Cadence |
| ---- | ----------- | ------- |
| Dial | 350 + 440 Hz | continuous |
| Busy | 480 + 620 Hz | 0.5 s on / 0.5 s off |
| Reorder (fast busy) | 480 + 620 Hz | 0.25 s on / 0.25 s off |
| Ringback | 440 + 480 Hz | 2 s on / 4 s off |
| Off-hook warning | 1400 + 2060 + 2450 + 2600 Hz | 0.1 s on / 0.1 s off |

DTMF digits per ITU-T Q.23.

## Interrupts

Per board profile; default polled.

## MAME files

- `src/mame/coinline/millennium_audio.cpp/h`

## Fixture files

None standalone; expected outputs captured in evidence bundles.

## Tests

- `tests/devices/test_alerter_dial_tone.cpp`
- `tests/devices/test_alerter_ringback.cpp`
- `tests/devices/test_alerter_busy.cpp`
- `tests/devices/test_alerter_dtmf.cpp`

## Boot milestone dependencies

- M5 first audio I/O observed.

## Acceptance criteria

- Each tone is generated at the spec'd frequency and cadence.
- DTMF digits match Q.23 dual-tone frequencies.
