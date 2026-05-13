# Boot source path to first VFD write

## Behavioral anchors (no proprietary artifacts cited)

| Stage | Role |
|-------|------|
| PIO bring-up | Sets **PIO_PORT_B** (`~VFD_CSB`), **PIO_PORT_H** (`UPPER_RAM_ENABLE` **0x07**), EPM power, etc. |
| Port map | **`DISPLAY_PORT`** for VFD; **`MACH_PIO` / `PIO_PORT_H`** @ **0xC0** = **`STATUS_PORT_3`** |
| Cash-box polling | Polls **`STATUS_PORT_3`** bits **2–3** for **cash box cover / removed** (`NEW_HARDWARE_REVISION_1`) |

## Intended order (high level)

1. Low-level **PIO** + **MMU** setup (seen in traces).
2. **Security / mechanical** status readable (**port 0xC0** reads).
3. **Telephony/modem/ASCI** and tasks advance until the **display task** reaches the VFD bootstrap / download path.

## Trace gap

- **No** `vfd_data` / port **0x60** in `io-trace.jsonl` → firmware **has not** entered **DISPLAY_PORT** write path.
- **Hypothesis addressed this pass:** **`in0(0xC0)`** returned **wrong** composite → **cash-box status** polling could **never** succeed when **shadow** had **bit 2 set** (e.g. after **`0x06`**).

## Emulator device

**`mach_pio` / STATUS_PORT_3** was the **most likely incomplete** board decode vs **ASCI** (second place once port H is fixed).

## Machine-readable

`build/generated/path-to-vfd-init.json`
