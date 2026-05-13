# Test plan — CPU and Z180 register

## Purpose

Verify the Z180 CPU core's instruction execution and internal-peripheral register behavior under firmware execution.

## Prerequisites

- Built emulator with the chosen Z180 core ([`../docs/z180-core.md`](../docs/z180-core.md)).
- Firmware binary; board profile.

## Fixtures

- `fixtures/board/board-profile-2line-vfd.json`
- Optional micro-firmware ROMs under `tests/devices/fixtures/` for unit-level CPU tests.

## Procedure

1. Run unit tests that exercise Z180 instruction edge cases and Z180-specific opcodes (MLT, IN0/OUT0, TST, OTIM(R), OTDM(R), SLP).
2. Run device-tier tests that program the MMU, ASCI 0, PRT 0, INT controller, DMA, refresh, and wait-state controller via firmware boot.
3. Compare registers at M4 against expected baselines.

## Expected behavior

- All Z80 + Z180 instructions decode and execute correctly.
- `CBR`/`BBR`/`CBAR` match firmware writes.
- `CNTLA0`/`CNTLB0`/`STAT0`/`ASTC0` match firmware writes.
- `TCR`/`RLDR0L/H` match firmware tick programming.
- `IL` base set; INT0 source masked correctly.
- `DSTAT`/`DMODE`/`DCNTL` match firmware DMA programming.
- `RCR` programmed.

## Pass criteria

- All unit + device tests pass.
- Z180 register dump at M4 matches expected baselines (or **OPEN QUESTION** rows in internal correlation notes are explicitly accepted).

## Fail criteria

- Any unit test fails.
- Register baseline mismatch without a documented OPEN QUESTION.

## Evidence artifacts

- Per-test register dump JSON.
- M4 milestone entry in `boot-trace.jsonl`.

## Source files touched

- `src/mame/coinline/millennium_state.cpp`

## Implementation files touched

- `tests/devices/test_z180_*.cpp`

## Automated test location

- `tests/devices/test_z180_mmu.cpp`
- `tests/devices/test_z180_asci.cpp`
- `tests/devices/test_z180_prt.cpp`
- `tests/devices/test_z180_int.cpp`
- `tests/devices/test_z180_dma.cpp`
- `tests/devices/test_z180_refresh.cpp`
- `tests/devices/test_z180_wait_states.cpp`
- `tests/devices/test_z180_icr.cpp`

## Cross-references

- [`../docs/z180-core.md`](../docs/z180-core.md), [`../docs/z180-internal-peripherals.md`](../docs/z180-internal-peripherals.md).

