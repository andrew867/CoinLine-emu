# mach_pio / port 0xC0 (PIO_PORT_H / STATUS_PORT_3)

## Hardware definition (port constants)

- `**MACH_PIO**` = **0xC0**
- `**PIO_PORT_H`** = `**MACH_PIO + 0**`
- `**STATUS_PORT_3**` = **0xC0** (same address — read path carries status inputs)
- **Cash box cover** bit: `**CASH_BOX_COVER_BIT`** = **0x04** (bit 2) — terminal poll semantics: **0 = shut** (good) for **NEW_HARDWARE_REVISION_1**
- **Cash box removed** bit: `**CASH_BOX_REMOVED_BIT`** = **0x08** (bit 3) — **0 = in place**

## Init (PIO bring-up)

`out0(PIO_PORT_H, UPPER_RAM_ENABLE)` where `**UPPER_RAM_ENABLE` = 0x07**.

## Firmware loop observation

`io-trace.jsonl` often shows `**mach_pio` write** `**0x06`** at `**0xC0**` — **bit0 clear** vs full **0x07**.

## Emulator behavior (after fix)

- **Write**: shadow **preserved**; trace tag `**mach_pio_port_h`** for offset 0.
- **Read**: `millennium_mach_pio_combine_port_h_read(shadow, smartcard&3)` — **clears bits 2–3** on read so **cash-box poll** **active-low OK** defaults apply; **smartcard** only merges **bits 0–1**.

## SRAM / upper-memory routing (glue parity)

Physical addresses **0xC0000–0xFFFFF** are resolved through **`millennium_mach_decode_phys_ram`** in **`millennium_mach_pio.cpp`**, using the **PIO_PORT_H** shadow (same latch as above). The CPU memory map invokes **`phys_ram_r/w`**, which consults that decoder rather than duplicating bank math in **`millennium_state.cpp`**.

## JSON map

`build/generated/mach-pio-c0-map.json`