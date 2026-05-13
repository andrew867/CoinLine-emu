# Spec — VFD device

This spec defines the VFD device contract. See [`../docs/vfd-emulation.md`](../docs/vfd-emulation.md) for narrative context.

## Purpose

Emulate the 2-line and 11-line VFD variants used by the Millennium-compatible terminal.

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Command port | W | Receives display commands. |
| Data port | W | Receives character bytes. |
| Status port | R | Returns busy / ready, optionally cursor position. |

Port numbers per board profile.

## I/O ports / memory regions

The VFD owns the ports declared in `fixtures/board/io-port-map.json` with `device = "vfd"`. No memory region.

## State machine

```
idle -> command_in_progress (busy_cycles) -> idle
idle -> data_in_progress    (busy_cycles) -> idle
```

## Timing behavior

| Operation | Cycles (typical) |
| --------- | ---------------- |
| Single character | configurable (`busy_cycles_char`) |
| Clear screen | configurable (`busy_cycles_clear`) |
| Cursor move | configurable (`busy_cycles_cursor`) |

## Interrupts

None by default.

## MAME files

- `src/mame/coinline/millennium_vfd.cpp`
- `src/mame/coinline/millennium_vfd.h`

## Fixture files

- `fixtures/display/vfd-2line-idle.json`
- `fixtures/display/vfd-11line-ad.json`

## Tests

- `tests/devices/test_vfd_command_decode.cpp`
- `tests/devices/test_vfd_buffer_snapshot.cpp`
- `tests/devices/test_vfd_2line_idle.cpp`
- `tests/devices/test_vfd_11line_ad.cpp`

## Boot milestone dependencies

- M5, M6, M10.

## Acceptance criteria

- Status port returns `busy` for `busy_cycles_*` after a command, then `ready`.
- Buffer snapshot matches the relevant fixture byte-for-byte.
- Both variants supported and selected by board profile.

## Engine-agnostic behavior contract

For the MIT-clean track, the same I/O ports, state machine, timing, and snapshot format apply.

## Reference parity program (reference program revision)

Closed requirements and enterprise test case IDs for bringing the implementation to **reference parity** (PIO **A0** demux, **display API header** DC vocabulary, clear semantics, font/bitmaps) live in:

- [`hardware/HW-VFD-reference program revision-PARITY.spec.md`](hardware/HW-VFD-reference program revision-PARITY.spec.md)
- [`../test-plans/hardware/HW-VFD-reference program revision-PARITY-tests.md`](../test-plans/hardware/HW-VFD-reference program revision-PARITY-tests.md)
