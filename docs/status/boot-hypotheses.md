# Boot hypotheses (pass/fail from latest trace-backed run)

Latest formal trace folder referenced in status docs: **`build/runs/20260504T094732-debug-boot-push/`** (firmware SHA-256 `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34`).

| # | Hypothesis | Result |
|---|------------|--------|
| 1 | **PC 0x0092** is real keypad code | **Pass** — `io-trace.jsonl` shows `pio_keypad` **write** to port `0x41` with `pc":"0x0092"` and data `0x80`. |
| 2 | MAME Z180 **AS_IO** map is hit for board + internal bus | **Pass** — full I/O log includes `z180_internal_bus` (ports `0x00`–`0x3F` catch-all) and board handlers. |
| 3 | Firmware stuck only because an interrupt never fires | **Inconclusive** — no single-PC spin identified in this pass; needs longer **cpu-trace** diff. |
| 4 | Stuck on **ASCI / timer** status | **Inconclusive** — internal ASCI ports appear; no proof of a wait loop on one bit yet. |
| 5 | **MMU** (CBR/BBR/CBAR) prevents reaching hardware-init / VFD init | **Inconclusive** — MMU programming visible; no `OUT` to `0x60` in 90 s run. |
| 6 | RAM not writable where firmware expects | **Fail as stated** — tracked window shows **0** RAM writes in M3 for keypad-first path; could mean firmware uses another store path or not yet. |
| 7 | Stack init source never runs | **Fail** — **SP** is **0x0000** at the keypad-preempt M3 snapshot in one ordering; later I/O shows **SP** `0x6D5D` (execution advanced). |
| 8 | **OUT0/IN0** not decoded | **Pass** — Z180 internal/external merge shows traffic on low ports in **io-trace**. |
| 9 | **DISPLAY_PORT** is **0x60** | **Pass** — board I/O headers define `DISPLAY_PORT 0x60`. |
|10 | **PIO_PORT_G** is **0x63** | **Pass** — matches driver map and observed PIO init traces. |

## Next engineering actions

1. Use **memory-trace.jsonl** + **cpu-trace.jsonl** to find why **no** `0x0060` / `vfd_data` lines appear before **90 s**.
2. Correlate long-run **PC** stable regions with `source-correlation.md` and generated correlation JSON when present.
3. If a tight **IN** loop on a board/VFD status bit appears, adjust that device model (not the milestone detector).
