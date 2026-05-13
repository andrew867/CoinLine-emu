# Z180 CPU core

This document specifies the CPU core expectations for `coinline-emu`. The MAME track uses MAME's `cpu/z180` device. The MIT-clean fallback track wires a permissive Z80 core to a custom Z180 internal-peripheral block. Either track must satisfy the contract here.

## Reference

- Hitachi HD64180 / Zilog Z180 datasheet (public).
- MAME `cpu/z180/z180.cpp` (BSD-3-Clause within MAME's tree; see [`engine-selection.md`](engine-selection.md)).

## Required functionality

| Feature | Required | Notes |
| ------- | -------- | ----- |
| Z80 instruction set | Yes | Including documented opcodes and Z180 extensions. |
| Z180 extension instructions (MLT, IN0/OUT0, TST, OTIM/OTIMR, OTDM/OTDMR, SLP) | Yes | Firmware uses these. |
| MMU (CBR / BBR / CBAR) | Yes | Selecting bank / common areas; physical address generation up to 1 MiB. |
| ASCI 0 and ASCI 1 | Yes | Modem on ASCI 0 (per default board profile); ASCI 1 may be per-profile. |
| PRT (programmable reload timers) 0 and 1 | Yes | At least one is used as the firmware tick. |
| Interrupt controller (IL register, IM2) | Yes | All interrupt sources documented in [`interrupt-map.md`](interrupt-map.md). |
| DMA (channels 0 and 1) | Yes | Used by table-storage and possibly DLA. |
| Refresh controller | Yes | At least to the extent that the firmware programs RCR. |
| Wait-state controller (DCNTL) | Yes | Per-region wait-state config from board profile. |
| HALT / SLP | Yes | Firmware uses SLP in the idle loop. |
| Trap on illegal opcodes | Yes | Logged via `millennium_debug.cpp`. |

## Clock

- Default crystal: per board profile (`z180.clock_hz`). Reference profiles use 12.288 MHz; SKU variants may use different crystals.
- The Z180's CPU runs at the crystal frequency divided by 2 unless the `CCR` post-scaler is reprogrammed by firmware.
- All timing in tests is expressed in **CPU cycles**, never in wall-clock units.

## Wait states

- Per-region wait states come from the board profile (`z180.wait_states.{rom,ram,io}`).
- `DCNTL` register defaults are mirrored from those settings at reset.
- Tests in `tests/devices/test_z180_wait_states.cpp` validate that the firmware's perceived wait-state counts match the board profile.

## Reset behavior

At reset:

- All registers initialized to Z180 reset defaults.
- PC = `0x0000`; SP undefined until firmware sets it.
- MMU registers `CBR = 0x00`, `BBR = 0x00`, `CBAR = 0xF0` (Common Area 1 from `0xF000`, Bank Area `0x0000`–`0xEFFF`, Common Area 0 zero-sized) per Z180 reset state.
- `I = 0x00`; IFF1 = IFF2 = 0; IM = 0.
- Internal-peripheral I/O base `ICR = 0x40`.

## Save state

The Z180 device must support save state for:

- All registers (BC, DE, HL, IX, IY, AF, SP, PC, I, R, BC', DE', HL', AF', IFF1, IFF2, IM).
- All internal-peripheral registers (MMU, ASCI 0/1, PRT 0/1, INT, DMA, Refresh, ICR, DCNTL).
- Pending interrupt state.
- Cycle counter.

Save state is required for regression replay (see [`debugging-guide.md`](debugging-guide.md)).

## Trace

The Z180 device must emit, on demand:

- Per-instruction PC + opcode (`-trace`).
- Per-I/O-port read/write with PC.
- Per-interrupt acknowledgment with vector and source.

## Performance

- The emulator is expected to run the firmware at real time on a current developer workstation. Performance is not a documented test gate; if performance regresses, tooling reports the regression but it does not block merges.

## Open questions

| Question | Resolution path |
| -------- | --------------- |
| Exact firmware crystal frequency | Resolved when board profile is finalized. |
| Whether the firmware ever reprograms the CPU clock divider | Resolved by inspecting CCR writes during boot trace. |
| Whether SLP is used by the idle loop | Resolved by M9 / M10 trace. |

## Cross-references

- [`z180-internal-peripherals.md`](z180-internal-peripherals.md).
- [`memory-map.md`](memory-map.md), [`io-port-map.md`](io-port-map.md), [`interrupt-map.md`](interrupt-map.md).
- [`engine-selection.md`](engine-selection.md).
