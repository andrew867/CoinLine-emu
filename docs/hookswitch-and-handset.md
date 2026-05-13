# Hookswitch and handset

This document describes the hookswitch (on-hook / off-hook) and handset audio loopback. Both are wired through the keypad / front-panel input subsystem but documented here separately because their semantics differ from key matrix scanning.

## Purpose

- Hookswitch — convey on-hook / off-hook state to the firmware so the call lifecycle progresses (lift → dial → connected → hangup).
- Handset audio — loop call-progress and side-band audio from the alerter / line interface to the simulated handset speaker.

## Firmware-facing interface

### Hookswitch

| Interface | Description |
| --------- | ----------- |
| Discrete input | A single bit read by the firmware indicating off-hook. |

Active level (high or low when off-hook) is per board profile.

### Handset audio

| Interface | Description |
| --------- | ----------- |
| Audio output | Mixed stream from alerter / call progress sources. |
| Volume | Per-board profile, adjusted by the volume keys. |

## State machine

### Hookswitch

```
on_hook (idle) <-> off_hook (active) -> in_call -> on_hook (idle)
```

Transitions between `off_hook` and `in_call` are firmware-driven; the device only reports the physical state of the switch.

## Timing

- Debounce: configurable in the device (`debounce_cycles` per spec).
- Audio sample rate: per board profile; common values 8 kHz / 16 kHz.

## Interrupts

Hookswitch may raise an interrupt on transition (per board profile). Default behavior is polled.

## Front-panel mapping

The clickable region in `artwork/millennium.lay` named `hook` toggles the hookswitch on each click. Holding `Ctrl` clicks lift the handset without releasing on the next click (for hold-and-test scenarios).

## Tests

- `tests/devices/test_hookswitch.cpp` — verifies on-hook / off-hook reads.
- `tests/devices/test_hookswitch_debounce.cpp` — verifies debounce behavior.
- `tests/devices/test_handset_audio_loopback.cpp` — verifies audio loopback in a known scenario.

## Boot-milestone dependencies

- M7: hookswitch read observed.
- M11: off-hook + dial sequence reaches the host bridge.

## Acceptance criteria

- The firmware sees a clean `lift -> hangup` cycle in `keypad-smoke.json`.
- Audio samples produced by the alerter reach the handset audio output sink.

## Cross-references

- [`keypad-emulation.md`](keypad-emulation.md).
- [`alerter-audio.md`](alerter-audio.md).
- [`status/TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md`](status/TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md) — chip-oriented TP backend: CP-visible hook transition vs steady sequencing when using the PCD3349A execution path.
- [`../test-plans/hookswitch-tests.md`](../test-plans/hookswitch-tests.md).
