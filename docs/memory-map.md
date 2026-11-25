# Memory map

## Current native driver

The Z180's physical program address space is 20 bits. The later-board
driver maps two 512 KiB NOR flash devices into that space and selects the
low 64 KiB bootstrap, download and main-program windows through the board
latch. The later-board SRAM is a separate 512 KiB banked device. The
`0xC0000–0xDFFFF` window selects a 128 KiB SRAM bank, and
`0xE0000–0xFFFFF` selects the next SRAM bank when the latch's low three
bits are 7, or flash otherwise. The serial EEPROM is accessed via I/O.
Flash, battery SRAM and EEPROM retain independently written contents.

The [current flash model and upgrade gates](firmware-download-storage.md)
describe the native driver. The fixture-based regions below document
the original synthetic harness and are retained for its tests. They do
not describe a separate native extended-address staging device.

The harness uses profiles described in [`board-profiles.md`](board-profiles.md)
and the fixture `fixtures/board/memory-map.json`. The fixture schema is in
[`../specs/memory-map.spec.md`](../specs/memory-map.spec.md).

## Address spaces

The Z180 has two distinct address spaces:

- **Program (memory) space** — 1 MiB physical via the MMU; 64 KiB logical.
- **I/O space** — 256 ports (Z80) plus the Z180's internal-peripheral I/O range mapped above port `0x40` by default.

This document covers program space. I/O space is in [`io-port-map.md`](io-port-map.md).

## Historical fixture region table

| Region | Logical / physical | Direction | Size | Purpose | Source of truth |
| ------ | ------------------ | --------- | ---- | ------- | --------------- |
| ROM | physical `0x00000` (linear image) | R | **1 MiB** (confirmed) | Firmware code + tables | Operator `flash.bin` must match **1048576** bytes; pinned in [`fixtures/board/memory-map.json`](../fixtures/board/memory-map.json) and [`fixtures/firmware/firmware-hashes.json`](../fixtures/firmware/firmware-hashes.json). |
| Reset / interrupt vectors | within ROM | R | small | Z180 IM2 vector table base addresses | Boot and interrupt tests |
| RAM (working) | extended linear fixture backing | R/W | **128 KiB** | Stack, heap, scratch, runtime state | [`fixtures/board/board-profile-2line-vfd.json`](../fixtures/board/board-profile-2line-vfd.json) |
| NVRAM | per fixture base | R/W | **8192** B | Configuration records, counters | Synthetic storage fixture |
| Table storage | per fixture base | R/W | **32 KiB** | Downloaded tables | Synthetic storage fixture |
| Firmware-download staging | per fixture base | R/W | **64 KiB** | DLA staging area | Synthetic storage fixture |
| Banked / MMU windows | logical | R/W | 64 KiB | MMU-mapped windows into physical | `z180-internal-peripherals.md` |

## Banking and MMU

The Z180 MMU divides the 64 KiB logical space into three regions: Common Area 0, Bank Area, and Common Area 1. The boundaries are programmable via `CBR`, `BBR`, and `CBAR` registers. Tests in `tests/devices/test_z180_mmu.cpp` exercise the MMU configuration. The native acceptance runner records the firmware's startup register snapshots.

## Vectors and stack

| Item | Notes |
| ---- | ----- |
| Reset vector | At physical `0x00000` (Z180 default). The ROM's first instruction is the start of firmware execution. |
| Interrupt vector base | Set via the Z180 `I` register and `IM2` vector table base. Pinned by firmware in early startup. |
| Stack | Initialized in startup code; pointer location is part of the M3 milestone trace. |
| Heap | If present, allocated within the working RAM region. |

## Native validation limits

| Question | Resolution path |
| -------- | --------------- |
| Exact ROM size per SKU | The acceptance inventory records sizes and hashes for each available image, including separate flash halves. |
| Exact RAM decode per SKU | The later-board bank selection is modeled and tested. The complete behavior of unavailable board variants remains unqualified. |
| Download recovery | Actual remote DLA transfer and interrupted-update recovery remain to be tested with the code server. |

## Memory-map JSON schema (informal)

The full schema is in [`../specs/memory-map.spec.md`](../specs/memory-map.spec.md). Informally, `fixtures/board/memory-map.json`:

```jsonc
{
  "address_spaces": ["program"],
  "regions": [
    { "name": "rom",         "base": "0x00000", "size": 524288, "access": "r",   "device": "rom"  },
    { "name": "ram",         "base": "0x80000", "size":  32768, "access": "rw",  "device": "ram"  },
    { "name": "nvram",       "base": "0x88000", "size":   8192, "access": "rw",  "device": "nvram" },
    { "name": "tablestore",  "base": "0x8A000", "size":  32768, "access": "rw",  "device": "nvram" },
    { "name": "dlastage",    "base": "0x92000", "size":  65536, "access": "rw",  "device": "nvram" }
  ]
}
```

Addresses above are illustrative — actual values come from board profiles.

## Cross-references

- [`board-profiles.md`](board-profiles.md) — per-profile sizes.
- [`z180-internal-peripherals.md`](z180-internal-peripherals.md) — MMU.
- [`nvram-and-table-storage.md`](nvram-and-table-storage.md), [`table-download-behavior.md`](table-download-behavior.md), [`firmware-download-storage.md`](firmware-download-storage.md).
