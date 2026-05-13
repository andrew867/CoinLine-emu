# Memory map

This document describes the address-space layout for `coinline-emu`. The exact base addresses and sizes are pinned per SKU in [`board-profiles.md`](board-profiles.md) and serialized to `fixtures/board/memory-map.json`. The schema is in [`../specs/memory-map.spec.md`](../specs/memory-map.spec.md). Internal source-map evidence is in ``.

## Address spaces

The Z180 has two distinct address spaces:

- **Program (memory) space** — 1 MiB physical via the MMU; 64 KiB logical.
- **I/O space** — 256 ports (Z80) plus the Z180's internal-peripheral I/O range mapped above port `0x40` by default.

This document covers program space. I/O space is in [`io-port-map.md`](io-port-map.md).

## Region table (representative; pinned per board profile)

| Region | Logical / physical | Direction | Size | Purpose | Source of truth |
| ------ | ------------------ | --------- | ---- | ------- | --------------- |
| ROM | physical `0x00000` (linear image) | R | **1 MiB** (confirmed) | Firmware code + tables | Operator `flash.bin` must match **1048576** bytes; pinned in [`fixtures/board/memory-map.json`](../fixtures/board/memory-map.json) and [`fixtures/firmware/firmware-hashes.json`](../fixtures/firmware/firmware-hashes.json). |
| Reset / interrupt vectors | within ROM | R | small | Z180 IM2 vector table base addresses | the firmware evidence inventory |
| RAM (working) | extended linear backing in fixture (**OPEN QUESTION** for silicon-accurate decode) | R/W | **128 KiB** in current profile | Stack, heap, scratch, runtime state | [`fixtures/board/board-profile-2line-vfd.json`](../fixtures/board/board-profile-2line-vfd.json); validate against trace-backed expectations (internal correlation notes if available). |
| NVRAM | per fixture base | R/W (battery-backed) | **8192** B in current profile | Configuration records, counters | **OPEN QUESTION** until EEPROM layout evidence is merged from internal map. |
| Table storage | per fixture base | R/W | **32 KiB** in current profile | Downloaded tables | **OPEN QUESTION** — see the firmware evidence inventory. |
| Firmware-download staging | per fixture base | R/W | **64 KiB** in current profile | DLA staging area | **OPEN QUESTION** — see the firmware evidence inventory. |
| Banked / MMU windows | logical | R/W | 64 KiB | MMU-mapped windows into physical | `z180-internal-peripherals.md` |

## Banking and MMU

The Z180 MMU divides the 64 KiB logical space into three regions: Common Area 0, Bank Area, and Common Area 1. The boundaries are programmable via `CBR`, `BBR`, and `CBAR` registers. Bring-up tests in `tests/devices/test_z180_mmu.cpp` exercise the MMU configuration the firmware actually uses. Until those tests pass, the emulator validates the MMU configuration by comparing the firmware's first writes to `CBR`/`BBR`/`CBAR` against the values in the firmware evidence inventory.

## Vectors and stack

| Item | Notes |
| ---- | ----- |
| Reset vector | At physical `0x00000` (Z180 default). The ROM's first instruction is the start of firmware execution. |
| Interrupt vector base | Set via the Z180 `I` register and `IM2` vector table base. Pinned by firmware in early startup. |
| Stack | Initialized in startup code; pointer location is part of the M3 milestone trace. |
| Heap | If present, allocated within the working RAM region. |

## Open questions

| Question | Resolution path |
| -------- | --------------- |
| Exact ROM size per SKU | **Resolved** for the canonical lab image at **1048576** bytes (see firmware hash registry). Other SKUs remain **OPEN QUESTION** until additional binaries are registered. |
| Exact RAM size per SKU | Resolved by board profile and confirmed by tests. |
| Whether table storage is contiguous or split | Resolved by the firmware evidence inventory. |
| Whether DLA staging overlaps NVRAM | Resolved by the firmware evidence inventory. Default assumption: separate region. |

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

