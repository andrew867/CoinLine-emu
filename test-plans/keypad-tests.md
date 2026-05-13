# Test plan — Keypad

## Purpose

Verify keypad matrix decoding, quick-access key handling, volume / language handling, and firmware acknowledgment of every declared key.

## Prerequisites

- Built emulator.
- Firmware; board profile.

## Fixtures

- `fixtures/board/board-profile-2line-vfd.json` (and 11-line for variant coverage).
- `fixtures/scenarios/keypad-smoke.json`.

## Procedure

1. Run matrix decode unit tests for every (row, column) pair.
2. Boot firmware to M7 (keypad scan observed).
3. Run `keypad-smoke.json` pressing every numeric key, every quick-access key, volume up/down, and language.
4. Capture firmware acknowledgment via VFD (e.g., displayed digit) or an internal counter exposed via debug.

## Expected behavior

- Every key produces the expected matrix or discrete signal.
- Firmware acknowledges every keypress (visible on VFD, or via debug counter).
- Quick-access keys trigger their per-profile actions.
- Volume up/down adjust handset volume; language toggles language state.

## Pass criteria

- All matrix decode unit tests pass.
- M7 reached.
- `keypad-smoke.json` passes with every key acknowledged.

## Fail criteria

- Any key missing an acknowledgment.
- Matrix decode produces wrong (row, column) pair.

## Evidence artifacts

- `vfd/snapshots/*.json` after each press.
- `io-trace.jsonl` showing matrix scans.

## Source files touched

- `src/mame/coinline/millennium_keypad.cpp/h`

## Implementation files touched

- `tests/devices/test_keypad_*.cpp`
- `tests/integration/test_keypad_smoke.cpp`

## Automated test location

- `tests/devices/test_keypad_matrix.cpp`
- `tests/devices/test_keypad_quick_access.cpp`
- `tests/devices/test_keypad_volume_language.cpp`
- `tests/devices/test_keypad_scan.cpp`
- `tests/integration/test_keypad_smoke.cpp`

## Cross-references

- [`../docs/keypad-emulation.md`](../docs/keypad-emulation.md), [`../specs/keypad-device.spec.md`](../specs/keypad-device.spec.md).
