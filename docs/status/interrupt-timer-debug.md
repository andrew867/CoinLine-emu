# Interrupt / timer debug (stub)

## Current trace evidence

- Sampled `cpu-trace.jsonl` in the M5-locked run shows **iff1: false** for many lines.
- I/O to **0x34** (`z180_itc`) and **0x36** (`z180_rcr`) appears in the keypad/MMU loop (`boot-blocker.md` excerpts).

## Next instrumentation

If MMU/memory correctness still leaves the loop: log **INT** accept, **IM** mode, **vector address**, `RETI`, and PRT/ASCI timer underflow to `build/generated/interrupt-timer-report.json` (not yet implemented in this pass).
