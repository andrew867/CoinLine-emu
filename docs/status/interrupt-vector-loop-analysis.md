# Interrupt / vector loop analysis (Z180, CoinLine)

## What we are proving

The firmware shows heavy sampling at **PC 0x0038** and in **0x00CF–0x00E5** with **IM1**-style **RST 38h** opcodes. This is consistent with the Z180 in **interrupt mode 1** (peripheral IRQ → **PC = 0x38**) and a **context-save** path at the **xentry** address, not a random tight loop in application code.

## Event-level tracing (2026-05+)

With `COINLINE_TRACE_VECTOR_EVENTS=1`, the driver emits:

- `interrupt-events.jsonl` — **EI/DI**, **IM0/1/2**, **RETI/RETN**, **RET**, **IFF1** edge samples (timer-based, **not** cycle-accurate).
- `vector-events.jsonl` — **RST 38h** opcode samples, **entry** when PC transitions onto **0x0038**.
- `context-switch-events.jsonl` — **enter/exit** for PC band **0x00CF–0x00E5** (`xentry_int_sub` interrupt glue).

## Answers (instrument-backed)

| Question | Answer |
| -------- | ------ |
| Is **0x0038** the IM1 vector? | **Yes** in the common case: **PC 0x0038** is the Z80/Z180 **mode 1** vector address. Traces should show `pc_enter_0x0038` when execution transitions onto **0x0038**. |
| Is **0x00CF–0x00E5** interrupt entry / save context? | **Correlated**: traces align **`xentry_int_sub`** at **0x000000CF** (absolute). The PC band matches interrupt subsystem entry, not arbitrary ROM. |
| RETI / RETN / RET | Observed via **`reti_opcode_sample`**, **`retn_opcode_sample`**, **`ret_opcode_sample`** in `interrupt-events.jsonl` (sampled; may miss very short windows). |
| IFF1 at instruction resolution? | **No** at true instruction boundaries; only **5 ms** `interrupt-trace.jsonl` and **10 µs** event probe. **iff1_rise_sampled** / **iff1_fall_sampled** show state changes between samples. |
| IRQ / NMI line in driver? | The CoinLine **machine driver** does not assert a fake **IRQ** line; on-chip **timers/ASCI** are the usual internal sources. **NMI** is not driven by the current driver. |
| Port **0x34** / **ITC** | Decoded in `z180-register-trace.jsonl` and **catch-all** internal reads as **`z180_itc`**. Read masking matches **`millennium_z180_itc_read_byte`** (host-side mirror of MAME `z180.cpp`). |

## Limits

- **Opcode sampling** at **10 µs** simulated time can miss single-cycle instructions between samples.
- **Interrupt acceptance** is **not** hooked from the CPU core; deduce from **PC @ 0x38**, **RETI**, and **IFF1** edges.

## Related files

- `build/generated/hot-vector-path.json`
- `docs/status/context-switch-path-analysis.md`
