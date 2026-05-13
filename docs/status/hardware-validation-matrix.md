# Hardware validation matrix (trace & artifact ↔ HW-ID)

**Purpose:** Map evidence files from mission Phase 7 to **hardware gap IDs**. Absence of a file does **not** automatically mean stub behavior — it may mean **feature inactive** in that run.

Legend: **R** = required when subsystem exercised; **O** = optional extended trace; **—** = not applicable.

| Artifact | HW IDs | Notes |
| -------- | ------ | ----- |
| `boot-milestones.json` / `boot-trace.jsonl` | HW-VFD-001, HW-KEY-001, global boot | M6/M10 milestones |
| `io-trace.jsonl` | HW-IO-001, HW-MACH-001, most peripherals | Unknown ports logged honestly |
| `memory-trace.jsonl` | HW-MMU-001, HW-MEM-002 | Logical vs physical |
| `mmu-translation-trace.jsonl` | HW-MMU-001 | Translation active |
| `interrupt-trace.jsonl` | HW-INT-001 | iff/im samples |
| `timer-trace.jsonl` | HW-TMR-001 | |
| `asci-trace.jsonl` | HW-ASCI-001 | |
| `uart-transcript.log` | HW-ASCI-001, HW-MOD-001 | |
| `reset-trace.jsonl` | HW-RES-001 | O |
| `z180-register-trace.jsonl` | HW-CPU-001, HW-INT-001 | O |
| `vfd-trace.jsonl` | HW-VFD-001, HW-VFD-002 | After driver emits |
| `voiceware-trace.jsonl` | HW-VOIC-001, HW-VOIC-002 | |
| `audio-trace.jsonl` | HW-SPK-001, HW-AUD-001, HW-VOIC-001 | Superset |
| `audio-route-trace.jsonl` | HW-AUD-001 | |
| `mute-route-trace.jsonl` | HW-AUD-002 (if split from route) | Maps to routing spec |
| `telephony-trace.jsonl` | HW-TEL-001 | |
| `supervision-trace.jsonl` | HW-SUP-001 | |
| `alerter-trace.jsonl` | HW-SPK-001, HW-DTMF-001 | |
| `card-trace.jsonl` | HW-CARD-001 | O until harness emits |
| `coin-trace.jsonl` | HW-COIN-001 | O |
| `nvram-trace.jsonl` | HW-NVRAM-001 | O |
| `host-bridge/transcript.jsonl` | HW-HB-001 | Evidence bundle layout |
| `*.wav` (voiceware) | HW-VOIC-001 | Non-silent proof |
| `screenshot*.png` | HW-VFD-001, HW-SCN-001 | Visual honesty |
| `evidence-summary.json` | HW-EV-001 | Run metadata |

## Validator scripts

- `tools/windows/validate-boot-milestones.ps1` — boot milestones (strict vs smoke).  
- Evidence bundle completeness — `specs/evidence-bundle.spec.md` + exporter.

## Field validation

Bench or PSTN-only checks remain **field_validation_pending** in JSON — they do not downgrade emulator code to **stub** if models are honest.
