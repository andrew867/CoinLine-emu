# Context-switch path analysis (0x00CF–0x00E5)

## Source correlation

- Trace correlation: interrupt save/restore entry **`xentry_int_sub`** at **0x000000CF** (absolute, **XSTART**).
- Disassembly at **0x00CF** with **DI**, **EX (SP),IX**, **PUSH** register set matches **interrupt / RTOS** **context save** patterns (save caller state, not a normal C function prologue only).

## Behavioral read

- **0x00CF–0x00E5** is **expected** to be **interrupt entry** / **supervisor** code that **eventually** returns via **RETI** or a control transfer back to the interrupted task, depending on the kernel design.
- **Return failure** (stack leak, **never leaving** the band) may indicate: **IRQ not cleared**, **timer status** never acknowledged, **wrong stack bank**, **bad TCB memory**, or **stuck IFF** — correlate `**interrupt-events.jsonl`** with `**stack-trace.jsonl**` and `**z180-register-trace.jsonl**`.

## Emulator traces

`COINLINE_TRACE_VECTOR_EVENTS=1` writes `**context_save_band_enter**` / `**context_save_band_exit**` when PC crosses into or out of **0x00CF–0x00E5**.

## Related

- `build/generated/context-switch-signature.json`
- `docs/status/rtos-context-switch-source-correlation.md`