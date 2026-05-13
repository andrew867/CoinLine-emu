# Test plan — Hookswitch

## Purpose

Verify the hookswitch (on-hook / off-hook) signal propagates to firmware, including debounce.

## Prerequisites

- Built emulator.
- Firmware; board profile.

## Fixtures

- `fixtures/board/board-profile-2line-vfd.json`.
- `fixtures/scenarios/keypad-smoke.json` (contains lift/hangup steps).

## Procedure

1. Run debounce unit test.
2. Boot firmware to M10.
3. Lift the handset; observe the firmware transition into off-hook flow (e.g., dial-tone audio).
4. Hang up; observe the firmware return to on-hook idle.

## Expected behavior

- On-hook reads idle state.
- Off-hook reads active state.
- Debounce filters bounce within `debounce_cycles`.
- Firmware enters off-hook flow when handset lifted.

## Pass criteria

- Debounce unit test passes.
- Lift / hangup observed via VFD or audio.
- M11 (host call attempted) reachable from off-hook.

## Fail criteria

- Bounce not filtered.
- Firmware does not transition on lift.

## Evidence artifacts

- `vfd/snapshots/*.json`.
- `audio/handset.wav` (optional).

## Source files touched

- `src/mame/coinline/millennium_keypad.cpp/h` (hookswitch is part of keypad device).

## Implementation files touched

- `tests/devices/test_hookswitch.cpp`
- `tests/devices/test_hookswitch_debounce.cpp`

## Automated test location

- `tests/devices/test_hookswitch.cpp`
- `tests/devices/test_hookswitch_debounce.cpp`
- `tests/devices/test_handset_audio_loopback.cpp`

## Cross-references

- [`../docs/hookswitch-and-handset.md`](../docs/hookswitch-and-handset.md).
- [`../docs/alerter-audio.md`](../docs/alerter-audio.md).
