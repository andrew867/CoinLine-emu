# Interrupt map

This document describes the interrupt sources, vectoring scheme, masks, and priorities for `coinline-emu`. Per-vector entries are pinned in `fixtures/board/interrupt-map.json`.

## Interrupt model

The Z180 supports three interrupt modes:

- **IM 0** — bus is sampled for an instruction (Z80-compatible).
- **IM 1** — single fixed vector at `0x0038`.
- **IM 2** — vectored. The high byte comes from the `I` register, the low byte from the data bus or from internal-peripheral vectoring.

The Millennium-compatible terminal firmware uses **IM 2** with the Z180's vectored internal-peripheral interrupts and external INT0–INT2 inputs. Vector base is set by `I`; per-source low bytes are programmed through Z180 ICR registers.

## Internal-peripheral interrupt sources

Per Z180 datasheet:

| Source | Mask register | Notes |
| ------ | ------------- | ----- |
| INT0 | external | `/INT0` pin; connected to the modem-related glue per the firmware evidence inventory. |
| INT1 | external | `/INT1` pin; per board profile. |
| INT2 | external | `/INT2` pin; per board profile. |
| PRT0 (Timer 0) | TCR.IE0 | Periodic timer; firmware uses for tick. |
| PRT1 (Timer 1) | TCR.IE1 | Periodic timer; firmware usage TBD. |
| DMA0 | DCNTL | DMA channel 0 completion. |
| DMA1 | DCNTL | DMA channel 1 completion. |
| ASCI0 (RX/TX) | STAT0.RIE/TIE | Modem UART. |
| ASCI1 (RX/TX) | STAT1.RIE/TIE | Auxiliary UART (per profile). |
| CSI/O | CNTR.EIE | Clocked serial — typically unused. |

## External interrupt sources

External `/INT0`, `/INT1`, `/INT2` lines connect to:

| Line | Source (representative; pinned per profile) |
| ---- | -------------------------------------------- |
| `/INT0` | Modem ring detect or DCD edge — see board profile. |
| `/INT1` | Per board profile. |
| `/INT2` | Per board profile. |

## Vector ordering

The Z180's internal-peripheral interrupts have a fixed priority order: INT0 highest, then INT1, INT2, PRT0, PRT1, DMA0, DMA1, CSI/O, ASCI0, ASCI1. This priority is honored by MAME's Z180 device. The firmware programs vector values; the emulator does not need to second-guess priority.

## Masks

| Mask | Holder | Notes |
| ---- | ------ | ----- |
| `I` register | Z180 | High byte of the IM2 vector. Set early in startup. |
| `IL` register | Z180 | Low-byte base for internal-peripheral vectors. |
| `IE` flag | Z180 | Master interrupt enable. |
| Per-peripheral enables | per peripheral | E.g. `STAT0.RIE` for ASCI0 RX. |

## Interrupt-map JSON schema (informal)

Full schema in `../specs/`-adjacent contracts. Informally, `fixtures/board/interrupt-map.json`:

```jsonc
{
  "mode": "im2",
  "vector_base_register": "I",
  "il_base_register": "IL",
  "sources": [
    { "source": "INT0",  "external_line": "INT0",   "owning_device": "modem",  "evidence": "the firmware evidence inventory" },
    { "source": "PRT0",  "external_line": null,     "owning_device": "z180",   "evidence": "datasheet" },
    { "source": "ASCI0", "external_line": null,     "owning_device": "modem",  "evidence": "datasheet" }
  ]
}
```

## Testing

- `tests/devices/test_z180_int.cpp` — verifies IM2 vector dispatch ordering.
- `tests/devices/test_z180_prt.cpp` — verifies PRT0 fires at the configured rate.
- `tests/devices/test_z180_asci.cpp` — verifies ASCI RX / TX interrupts trigger handlers.
- `tests/devices/test_modem_state_machine.cpp` — verifies INT0 transitions modem state correctly.

## Open questions

| Question | Resolution path |
| -------- | --------------- |
| Exact INT1/INT2 sources | the firmware evidence inventory once authored. |
| Priority quirks under DMA | Verified by `test_z180_dma.cpp`. |
| Does CSI/O ever fire? | If never used by firmware, keep it disabled and assert in CI. |

## Cross-references

- [`z180-core.md`](z180-core.md), [`z180-internal-peripherals.md`](z180-internal-peripherals.md).
- [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md).
