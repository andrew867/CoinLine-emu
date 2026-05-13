# Keypad emulation

This document describes the keypad device, including the numeric matrix, quick-access keys, volume keys, and language key. Hookswitch is documented separately in [`hookswitch-and-handset.md`](hookswitch-and-handset.md). Lock / door / vault / service inputs are in [`lock-door-vault-service.md`](lock-door-vault-service.md).

## Purpose

Translate user input from the front-panel layout (clickable regions in `artwork/millennium.lay`) into the keypad-matrix and discrete-input signals the firmware reads.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Matrix scan | Firmware drives column lines and reads row lines (or vice versa). |
| Discrete inputs | Volume up, volume down, language, quick-access keys (per profile). |

Port numbers come from the active board profile's I/O port map.

## State machine

```
idle -> column_select -> row_read -> column_select -> ...
```

Debounce is handled either in the firmware or in the device per the board profile (`keypad.debounce_cycles`). The default is firmware-side debounce.

## Timing

- The firmware scans the matrix at PRT0's tick rate.
- Every column-select / row-read cycle is bounded by `scan_cycles` per the spec.

## Interrupts

By default keypad scanning is polled. Some board profiles route a discrete key (e.g., service switch) to `/INT2`; see [`interrupt-map.md`](interrupt-map.md) and [`lock-door-vault-service.md`](lock-door-vault-service.md).

## Quick-access keys

The reference profile has the following quick-access keys (per board profile):

- Operator
- Information
- 411
- 911 (or local emergency)
- Coin return

Each is wired to a discrete input bit per the spec.

## Volume and language

- Volume up / down — discrete inputs that the firmware reads at scan time.
- Language — toggles between configured display languages.

## Fixtures

Keypad scenarios use the press_key verb in [`scenario-runner.md`](scenario-runner.md). No standalone fixture file is required for the keypad itself.

## Tests

- `tests/devices/test_keypad_matrix.cpp` — verifies matrix decode for every row/column combination.
- `tests/devices/test_keypad_quick_access.cpp` — verifies each quick-access key.
- `tests/devices/test_keypad_volume_language.cpp` — verifies volume up/down and language.
- `tests/devices/test_keypad_scan.cpp` — verifies firmware-side scan rate.

## Boot-milestone dependencies

- M5: first keypad I/O read observed.
- M7: keypad scan observed.

## Acceptance criteria

- Every declared key produces the expected matrix or discrete signal.
- Firmware acknowledges all key events per scenario `keypad-smoke.json`.

## Cross-references

- [`../specs/keypad-device.spec.md`](../specs/keypad-device.spec.md).
- [`../test-plans/keypad-tests.md`](../test-plans/keypad-tests.md).
- [`hookswitch-and-handset.md`](hookswitch-and-handset.md).
