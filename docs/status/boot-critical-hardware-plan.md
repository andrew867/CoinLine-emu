# Boot-critical hardware plan

**Goal:** Reproducible path from power-on to **M6** (first firmware-driven `DISPLAY_PORT` **0x60** write) and, when evidence supports it, **M10** idle/out-of-service.

**Non-goals (until M6):** coin, magstripe, smartcard polish, ADSI, artwork click maps, evidence-bundle polish as primary work.

**Principles**

1. No **fake** VFD text, modem bytes, NCC frames, or voice phrase labels.  
2. Traces and milestones must reflect **real** CPU and I/O behavior.  
3. Each tranche: **build → 180 s capture → compare** to `build/runs/20260504T135115-post-mmu-boot-gate`.  
4. If blocked, write **concrete** next fix in `boot-blocker.md` (emitted in run dir on stop) and in [`boot-critical-final-status.md`](boot-critical-final-status.md).

**Tranche map**

| ID | Focus | Primary HW IDs |
| -- | ----- | --------------- |
| B0 | Baseline repo + run record | — |
| B1 | IRQ/timer/reset traces | HW-INT-001, HW-TMR-001, HW-RES-001 |
| B2 | ASCI/modem readiness | HW-ASCI-001, HW-MOD-001, HW-HB-001 if proven |
| B3 | MACH PIO latch | HW-MACH-001 |
| B4 | Voiceware gate | HW-VOIC-001, HW-VOIC-002 |
| B5 | NVRAM only if trace proves pre-VFD access | HW-NVRAM-001 |
| B6 | VFD proof / M6 | HW-VFD-001 |
| B7 | M10 push | HW-VFD-001 |

See [`boot-critical-runbook.md`](boot-critical-runbook.md) for commands.
