# Boot loop analysis (trace-backed)

## Source run

`build/runs/20260504T132707-z180-visual-fix/`

## Answers (from traces, not guesses)

1. **Reset/startup re-entry** — **Not** observed as a full CPU reset loop. `cpu-trace.jsonl` shows long runs with **iff1 false**; milestone sequence in `boot-trace.jsonl` is M0..M5 in order.
2. **PC 0x0171** — M2 shows **0x0171** as the post-reset jump target. Later **0x0174** appears in `io-trace` (ITC read), not a periodic full reset to 0x0171.
3. **PC 0x0092** — **Yes**, appears with `pio_keypad` and `M5` (keypad path).
4. **SP** — **Alternates** between **0x6D57** and **0xCFC8 / 0xCFBE** (see `boot-blocker.md` excerpts). Not monotonic drift across samples.
5. **Stack in RAM** — After MMU translation, **logical SP** maps to **physical** addresses. With **BBR=0**, logical **0x6D57** → physical **0x06D57**. Writable SRAM is expected at **physical ≥ 0xC0000**. Until the driver overlay fix, **stores** to **physical &lt; 0xC0000** were **silent drops** (flat ROM mapping).
6. **RAM persistence** — **128 KiB** window **0xC0000–0xDFFFF** (`phys_ram_w`) persisted; **low memory** did **not** prior to overlay.
7. **memory-trace.jsonl** — That run’s file was **empty** (artifact present but no writes recorded through `phys_ram_w` path).
8. **Interrupts** — **iff1** stays **false** in sampled `cpu-trace` lines (not a proof of no IRQ, but no `EI` path in samples).
9. **Watchdog** — **No** direct evidence in these artifacts.
10. **Wrong bank** — **Not** “CPU MMU off”. MAME’s Z180 **does** translate (see `z180ops.h` `MMU_REMAP_ADDR`). The bug was **board map discarding** stores outside SRAM.

## Machine-readable signature

`build/generated/boot-loop-signature.json`
