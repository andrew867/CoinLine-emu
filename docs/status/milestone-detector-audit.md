# Boot milestone detector audit (current)

## M0–M2

- **M0**: firmware hash and size at load time (unchanged).
- **M1 / M2**: static reset vector read from `flash` region (unchanged).

## M3 / M4

- **M3** must reflect RAM-write reality where possible. It is emitted together with **M4** (Z180 register snapshot) on the **first** of:
  - **`first_ram_write`**: first write to the tracked physical RAM window (`0xC0000`–`0xDFFFF`).
  - **`keypad_access_before_first_ram_write`**: first keypad PIO access occurred before any counted RAM write (forces correct ordering vs **M5**).
  - **`timeout_no_ram_write_yet`**: 50 ms emulated time elapsed with no RAM write — honest **ram_writes = 0** with non-zero **SP** once execution has advanced.

The **`m3_trigger`** field in `boot-trace.jsonl` records which path fired.

## M5 (keypad)

- **Only** the keypad PIO handlers (`pio_keypad_r` / `pio_keypad_w`, ports `0x41`–`0x44`) advance **M5**.
- **Removed** false positives from board status, VFD, coin, audio, smartcard, etc.

## M6 / M7–M10

- **M6**: still **first firmware-visible VFD model milestone** after real writes to `DISPLAY_PORT` (`0x60`) via `vfd_display_w` (unchanged semantics).
- **M7–M10**: unchanged logic (keypad scan counter, modem ASCI, scheduler PC, idle fixture).

## Validator

- Full CI expectation remains **M0–M10** in `validate-boot-milestones.ps1` (no **M10** → failure).
- **`BootTraceSmoke`**: development-only check for **M0–M5** order and presence.
- **M5–M9** may appear in any order after **M4**; **M10** must be after **M4** and after the latest of **M5**–**M9** that exist.
