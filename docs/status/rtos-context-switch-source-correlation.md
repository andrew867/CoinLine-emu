# RTOS / interrupt source correlation

## 0x0038 — interrupt vector (mode 1)

On Z80-compatible CPUs, **interrupt mode 1** forces **PC = 0x0038** on maskable interrupt acknowledge. Firmware spending time at **0x0038** is **normal** for IRQ dispatch, not proof of a bug by itself.

## 0x00CF — `xentry_int_sub`

**PC in 0x00CF–0x00E5** aligns with the interrupt subsystem entry band used by the executive (RTOS interrupt glue), **not** arbitrary ROM padding — consistent with trace tags naming **`xentry_int_sub`** at **0xCF**.

## Voiceware vs RTOS

Voiceware phrase writes (**port 0x0061**, symbol **`VOICE_SYNTHESIS_CODE_ADDR`**) can occur **before** VFD (**0x0060**) during boot or call-flow. **M5V** records the **first** phrase write; it does **not** replace **M6** (VFD).

## Hardware control (`HW_CNTL_PORT` **0x40**)

- **`VOICE_SYNTHESIS_RESET`** = **0x08** on **`HW_CNTL_PORT`** (with **SCLK** on bit 7 per init).
- The emulator now **latches** writes to **0x40** in **`m_hw_cntl_port_image`** and merges **readback** with the **security** low nibble so **control bits** are not stuck at **`| 0xF0`** with **discarded writes**.

## Still blocked?

If **`context_save_band_enter`** repeats without **`context_save_band_exit`** and **`RETI`** samples are rare, treat as **IRQ/timer/voiceware** interaction — consult **`interrupt-events.jsonl`**, **`timer-trace.jsonl`**, and **`voiceware-trace.jsonl`**.
