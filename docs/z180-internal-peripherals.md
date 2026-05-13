# Z180 internal peripherals

This document describes the Z180 on-chip peripherals as `coinline-emu` uses them: MMU, ASCI 0/1, PRT 0/1, interrupt controller, DMA, refresh, and wait-state controller. It complements [`z180-core.md`](z180-core.md).

## I/O base

The Z180's internal-peripheral I/O range is moved by `ICR` (`I/O Control Register`). Default `ICR = 0x40`, which places the internal peripherals at I/O ports `0x40`–`0x7F`. Firmware may relocate this base; the emulator follows.

## MMU

| Register | Address | Purpose |
| -------- | ------- | ------- |
| `CBR` | base+0x38 | Common Area 1 base (8-bit) |
| `BBR` | base+0x39 | Bank Area base (8-bit) |
| `CBAR` | base+0x3A | Common / Bank Area boundaries |

Bring-up: tests in `tests/devices/test_z180_mmu.cpp` exercise the MMU configuration the firmware writes during startup.

## ASCI 0 and ASCI 1

| Register | Address (channel 0) | Address (channel 1) | Purpose |
| -------- | ------------------- | ------------------- | ------- |
| `CNTLA` | base+0x00 | base+0x01 | Mode, RTS, MPBR / EFR |
| `CNTLB` | base+0x02 | base+0x03 | Baud rate; clock source |
| `STAT` | base+0x04 | base+0x05 | RX/TX status, error bits |
| `TDR` | base+0x06 | base+0x07 | Transmit data |
| `RDR` | base+0x08 | base+0x09 | Receive data |
| `ASEXT` | base+0x12 | base+0x13 | Extension control (Z180 family-specific) |
| `ASTC` | base+0x1A/0x1B | base+0x1C/0x1D | Time constant for baud generator |

ASCI 0 carries the modem traffic. ASCI 1 may be used by some board profiles. See [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md).

## PRT 0 and PRT 1

| Register | Address | Purpose |
| -------- | ------- | ------- |
| `TMDR0L/H` | base+0x0C/0x0D | Timer 0 data |
| `RLDR0L/H` | base+0x0E/0x0F | Timer 0 reload |
| `TCR` | base+0x10 | Timer control (TIE0, TIE1, TDE0, TDE1, TIF0, TIF1) |
| `TMDR1L/H` | base+0x14/0x15 | Timer 1 data |
| `RLDR1L/H` | base+0x16/0x17 | Timer 1 reload |
| `FRC` | base+0x18 | Free-running counter (in some Z180 variants) |

The firmware uses at least PRT0 as a periodic tick.

## Interrupt controller

See [`interrupt-map.md`](interrupt-map.md). The `IL` register holds the low-byte base for vectored internal-peripheral interrupts.

## DMA

| Register | Purpose |
| -------- | ------- |
| `SAR0L/H/B` | DMA0 source address |
| `DAR0L/H/B` | DMA0 destination address |
| `BCR0L/H` | DMA0 byte count |
| `MAR1L/H/B` | DMA1 memory address |
| `IAR1L/H` | DMA1 I/O address |
| `BCR1L/H` | DMA1 byte count |
| `DSTAT` | DMA status (DE0/DE1, DWE0/DWE1, DIE0/DIE1) |
| `DMODE` | DMA mode |
| `DCNTL` | DMA control + wait states |

## Refresh

| Register | Purpose |
| -------- | ------- |
| `RCR` | Refresh control (REFE, REFW, CYC1, CYC0) |

## Wait states

| Register | Purpose |
| -------- | ------- |
| `DCNTL` | Holds memory/IO wait states (MWI1/MWI0/IWI1/IWI0). |
| `RCR` | Refresh adds bus cycles. |

## Bring-up checklist

| Step | Test |
| ---- | ---- |
| Verify `ICR` placement matches firmware writes. | `tests/devices/test_z180_icr.cpp` |
| Verify `CBR`/`BBR`/`CBAR` match firmware writes during M3/M4. | `tests/devices/test_z180_mmu.cpp` |
| Verify `CNTLA0`/`CNTLB0`/`STAT0`/`ASTC0` match firmware writes during M8. | `tests/devices/test_z180_asci.cpp` |
| Verify `TCR`/`RLDR0L/H` match firmware tick programming. | `tests/devices/test_z180_prt.cpp` |
| Verify `IL` base. | `tests/devices/test_z180_int.cpp` |
| Verify `DSTAT`/`DMODE`/`DCNTL` programming for table-storage / DLA paths. | `tests/devices/test_z180_dma.cpp` |
| Verify `RCR` programming. | `tests/devices/test_z180_refresh.cpp` |

## What MAME provides

MAME's `cpu/z180` device implements all of the above. The CoinLine machine driver does **not** need to re-implement the internal peripherals; it wires the ASCI's serial transmit / receive to the modem device, the PRT's interrupt to the interrupt controller, and the DMA's memory transfers to the address map.

## What the MIT-clean fallback track must provide

In the fallback track, the project authors a `coinline_emu_z180_peripherals.cpp/h` module that implements the same register set per the public Z180 datasheet. The bring-up checklist tests are engine-agnostic.

## Open questions

| Question | Resolution path |
| -------- | --------------- |
| Does the firmware use ASCI 1? | Inspect the boot/IO trace for ASCI 1 register writes; if absent, the modem is on ASCI 0 only. |
| Does the firmware reprogram `ICR`? | Boot trace. |
| Which DMA channel handles table storage? | the firmware evidence inventory. |

## Cross-references

- [`z180-core.md`](z180-core.md).
- [`memory-map.md`](memory-map.md), [`io-port-map.md`](io-port-map.md), [`interrupt-map.md`](interrupt-map.md).
