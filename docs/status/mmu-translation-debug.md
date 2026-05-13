# MMU translation debug

## CPU core

MAME `z180_device` performs translation inside opcode/memory accessors (`MMU_REMAP_ADDR` in `z180ops.h`). The program bus uses **20-bit physical** addresses.

## Board driver

- **No second MMU** in the driver. `millennium_z180_mmu.*` **replicates** the same table for **diagnostics, tests, and JSONL** (`mmu-translation-trace.jsonl`).
- Register visibility matches **CPU state** (`millennium_z180_trace_read_byte`).

## Formula reference

`millennium_z180_mmu_build_table` copies **MAME** `z180_mmu()`:

- **CBAR** splits bank vs common (**low nibble** = bank split, **high nibble** = common split).
- **CBR** offsets common-area logical pages.
- **BBR** offsets bank-area logical pages.

## Reports

- `build/generated/mmu-translation-report.json`
- Runtime: set `COINLINE_TRACE_MMU_TRANSLATION=1` (and paths via `run-screenshot-capture.ps1`) for `mmu-translation-trace.jsonl`.
