# Test plan — VFD

## Purpose

Verify VFD command decoding, buffer rendering, and idle-display equivalence for both 2-line and 11-line variants.

## Prerequisites

- Built emulator.
- Firmware; board profile selecting the VFD variant under test.

## Fixtures

- `fixtures/display/vfd-2line-idle.json`
- `fixtures/display/vfd-11line-ad.json`
- `fixtures/board/board-profile-2line-vfd.json`
- `fixtures/board/board-profile-11line-vfd.json`

## Procedure

1. Run command decode unit tests against synthetic command streams.
2. Boot firmware to M6 (first VFD write).
3. Run `boot-to-idle.json` and capture the VFD final buffer.
4. Compare against the relevant fixture.

## Expected behavior

- Each declared command transitions the VFD state machine correctly and updates the buffer.
- Status port reports `busy` for `busy_cycles_*` then `ready`.
- Final buffer matches fixture byte-for-byte at M10.

## Pass criteria

- All decode unit tests pass.
- M6 reached deterministically.
- M10 buffer matches the relevant fixture.

## Fail criteria

- Decode produces wrong buffer state.
- M6 not reached.
- Final buffer mismatch.

## Evidence artifacts

- `vfd/final.json`
- `vfd/snapshots/*.json`

## Source files touched

- `src/mame/coinline/millennium_vfd.cpp/h`

## Implementation files touched

- `tests/devices/test_vfd_*.cpp`

## Automated test location

- `tests/devices/test_vfd_command_decode.cpp`
- `tests/devices/test_vfd_buffer_snapshot.cpp`
- `tests/devices/test_vfd_2line_idle.cpp`
- `tests/devices/test_vfd_11line_ad.cpp`

## Cross-references

- [`../docs/vfd-emulation.md`](../docs/vfd-emulation.md), [`../specs/vfd-device.spec.md`](../specs/vfd-device.spec.md).
