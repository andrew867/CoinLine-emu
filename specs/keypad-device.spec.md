# Spec — Keypad device

This spec defines the keypad device contract. See [`../docs/keypad-emulation.md`](../docs/keypad-emulation.md) for context.

## Purpose

Emulate the numeric keypad matrix, quick-access keys, volume keys, language key, and hookswitch.

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Column select | W | Drives column lines. |
| Row read | R | Reads row lines. |
| Discrete inputs | R | Volume up/down, language, quick-access keys. |

## I/O ports

Per `fixtures/board/io-port-map.json` rows with `device = "keypad"`.

## State machine

```
idle -> column_select -> row_read -> idle
```

## Timing behavior

- `scan_cycles` per matrix scan iteration (per spec / board profile).
- `debounce_cycles` for discrete inputs.

## Interrupts

By default polled. Service switch may map to `/INT2` per board profile.

## MAME files

- `src/mame/coinline/millennium_keypad.cpp/h`

## Fixture files

None (driven by scenario verbs `press_key`, `lift_handset`, `hang_up`).

## Tests

- `tests/devices/test_keypad_matrix.cpp`
- `tests/devices/test_keypad_quick_access.cpp`
- `tests/devices/test_keypad_volume_language.cpp`
- `tests/devices/test_keypad_scan.cpp`

## Boot milestone dependencies

- M5, M7.

## Acceptance criteria

- Each declared key produces the expected matrix or discrete signal.
- Firmware acknowledges all keys per `keypad-smoke.json`.
