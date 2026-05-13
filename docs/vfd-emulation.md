# VFD emulation

This document describes the Vacuum Fluorescent Display (VFD) device. The Millennium-compatible terminal supports a 2-line VFD (the common variant) and an 11-line VFD (the optional variant). Both are emulated by the same device class with a `variant` field.

## Purpose

Render text and graphic commands the firmware writes to the display. Capture the resulting buffer for evidence bundles.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Command port | Receives display commands (cursor, clear, custom characters). |
| Data port | Receives character bytes. |
| Status port | Returns busy / ready (and optionally cursor position). |

Exact port numbers come from the active board profile's I/O port map (`fixtures/board/io-port-map.json`).

## State machine

```
idle -> command_in_progress -> idle
idle -> data_in_progress    -> idle
```

The VFD parses an opcode-prefixed command stream similar to common VFD controllers. Specific opcodes are pinned in the firmware evidence inventory and reflected in the spec under `../specs/vfd-device.spec.md`.

## Timing behavior

- Each command consumes a configurable number of CPU cycles (`busy_cycles`).
- The status port reports `busy` until the command completes.

## Interrupts

The VFD does not normally raise interrupts in this terminal family.

## Variants

| Variant | Size | Notes |
| ------- | ---- | ----- |
| 2-line | 20 columns × 2 rows | Default; supports basic text + cursor control. |
| 11-line | 20 columns × 11 rows | Supports advertising frames and richer formatting. |

## Buffer snapshots

The VFD device maintains a text buffer and a raw command stream. Snapshots are exported as part of evidence bundles:

```jsonc
{
  "variant": "2line",
  "rows": 2,
  "columns": 20,
  "text": ["INSERT $1.00      ", "OR SWIPE CARD     "],
  "raw": "0x1B 0x40 ..."
}
```

## Fixtures

- `fixtures/display/vfd-2line-idle.json` — idle screen for 2-line variant.
- `fixtures/display/vfd-11line-ad.json` — advertising frame for 11-line variant.

## Tests

- `tests/devices/test_vfd_command_decode.cpp` — decode known commands.
- `tests/devices/test_vfd_buffer_snapshot.cpp` — snapshot equivalence.
- `tests/devices/test_vfd_2line_idle.cpp` — boot-to-idle pinned snapshot.
- `tests/devices/test_vfd_11line_ad.cpp` — advertising frame pinned snapshot.

## Boot-milestone dependencies

- M5: first VFD I/O write observed.
- M6: VFD command decoded; buffer non-empty.
- M10: idle display matches reference.

## Acceptance criteria

- All declared commands are decoded.
- Both variants render the supplied reference fixtures byte-for-byte.
- Buffer snapshot matches `fixtures/display/*.json` deterministically.
- Status port returns `ready` after `busy_cycles` for every command.

## Cross-references

- [`../specs/vfd-device.spec.md`](../specs/vfd-device.spec.md).
- [`../test-plans/vfd-tests.md`](../test-plans/vfd-tests.md).
