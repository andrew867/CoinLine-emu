# Z180 internal register I/O trace (CoinLine driver)

## Summary

The **Z80180** uses `extended_io == false`: internal-control registers appear in a **0x40-port window** selected by **IOCR** (see `z180_device::is_internal_io_address` in MAME `z180.cpp`).

MAME’s CPU implements real internal state (`state_int(Z180_*)`). On **IN** from an internal port, `z180_readcontrol()` performs an external bus read (for observation) then returns **`z180_internal_port_read()`** — the value returned to the program is **not** the raw external open-bus byte.

The CoinLine driver’s **catch-all** I/O map previously logged misleading bytes for internal ports. It now:

1. Detects internal ports with **`millennium_z180_port_is_internal_window()`** (same mask as the CPU: `(port ^ IOCR) & 0xFFC0 == 0` on Z80180).
2. Logs **`millennium_z180_trace_read_byte()`**, which mirrors **masked read formulas** from `z180_internal_port_read()` for MMU (0x38–0x3A), ITC, RCR, IOCR, OMCR, DMA regs where implemented, etc.
3. Uses trace tags such as **`z180_mmu_bbr`**, **`z180_mmu_cbr`**, **`z180_mmu_cbar`**, **`z180_itc`**, **`z180_internal_unknown`**.

## Observed ports (firmware trace)

| Full port (typical) | Low 6 bits | Register | Trace tag |
|----------------------|-------------|----------|-----------|
| 0x0034 | 0x34 | ITC | `z180_itc` |
| 0x0036 | 0x36 | RCR | `z180_rcr` |
| 0x0038 | 0x38 | CBR | `z180_mmu_cbr` |
| 0x0039 | 0x39 | BBR | `z180_mmu_bbr` |
| 0x003A | 0x3A | CBAR | `z180_mmu_cbar` |
| 0x003F | 0x3F | IOCR | `z180_iocr` |

**BBR readback:** After firmware programs BBR, **`io-trace.jsonl`** read lines for port **0x0039** should show **`data` matching `state_int(Z180_BBR)`** (e.g. `0x4D`), not a stuck **`0xFF`**, as long as the line is classified as an internal register trace (tag `z180_mmu_bbr`).

## Machine-readable map

See **`build/generated/z180-internal-register-map.json`** (generated/maintained alongside the driver).

## Source evidence

- MAME: `third-party/mame/src/devices/cpu/z180/z180.cpp` — `z180_internal_port_read`, `z180_readcontrol`.
- Firmware: board I/O include / hardware-init modules — MMU and board I/O constants (paths under internal docs only).
- Driver: `src/mame/coinline/millennium_z180_internal.cpp`, `millennium_z180_register_math.h`, `millennium_state.cpp` (`catch_all_io_*`).
