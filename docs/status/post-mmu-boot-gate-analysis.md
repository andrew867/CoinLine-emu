# Post-MMU boot gate analysis

## Evidence run

`build/runs/20260504T133903-mmu-memory-fix/`

Machine-readable summaries:

- `build/generated/hot-pc-frequency.json`
- `build/generated/hot-port-frequency.json`
- `build/generated/post-mmu-boot-loop-signature.json`

## Findings

1. **Hottest PCs** cluster in **logical `0x00C0–0x00E5`** (hundreds of hits per 45 s window) — tight subroutine/ISR-style band, **not** the `0x30xx` MMU configuration cluster alone.
2. **Hottest I/O**: **MMU** (`0x38–0x3A`, `0x39`), then **ITC/IOCR/RCR**, **keypad** `0x41`, **`mach_pio` `0xC0`**, **voice** `0x61`.
3. **EI**: sampled **`iff1` false** across `cpu-trace.jsonl` — no proof interrupts are enabled in the sampled window.
4. **RETI/RETN**: not extracted in this pass (requires opcode trace or CPU hook); **next** if still blocked.
5. **Timer**: Z180 `TCR`/`RLDR0` appear in `z180-register-trace.jsonl` snapshots; **dedicated** `timer-trace.jsonl` added when `COINLINE_TRACE_TIMERS=1`.
6. **ASCI**: `STAT0`/`CNTL` visible in M4-style snapshots; **dedicated** `asci-trace.jsonl` with `COINLINE_TRACE_ASCI=1`.
7. **Port 0xC0**: one write per outer loop (`0x06` common) — **read** must satisfy **cash-box status** checks on **bits 2–3** (see `mach-pio-c0-debug.md`).

## Blocker fixed this pass (code)

**PIO_PORT_H / STATUS_PORT_3 @ 0xC0** read model: previous driver replaced the latch low nibble entirely with `smartcard::status_lines()`, clobbering **UPPER_RAM_ENABLE** latch semantics and **never** presenting **cash-box** **bit 2/3** defaults. **Implemented** `millennium_mach_pio_combine_port_h_read()` to **clear status bits 2–3** on read (kiosk-safe) and merge only **smartcard** **bits 0–1**.

## If M6 still missing

Next candidates: **ASCI gating** (host/modem not ready), **EI + timer IRQ** not taken, **install/DLA** path — use new `interrupt-trace.jsonl` / `timer-trace.jsonl` / `asci-trace.jsonl`.
