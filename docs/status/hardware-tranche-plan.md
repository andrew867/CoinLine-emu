# Hardware tranche plan (H0–H14)

Each tranche lists purpose, prerequisites, hardware IDs, primary files, tests, run commands, evidence, acceptance criteria, and **expected commit message prefix**.

Reference gap IDs: [`hardware-gap-register.json`](hardware-gap-register.json).

---

## Tranche H0 — Audit, cleanup, commit discipline

| Field | Content |
| ----- | ------- |
| **Purpose** | Establish measurable baseline; no feature claims without traces. |
| **Prerequisites** | None. |
| **Hardware items** | HW-IO-001, gap register metadata, CI doc pointers. |
| **Files to create** | `docs/status/hardware-*.md`, `hardware-gap-register.json`, `specs/hardware/*`, `test-plans/hardware/*`. |
| **Files to modify** | Optional: `docs/README.md` index link (separate doc-only commit). |
| **Tests** | Existing `ctest` smoke; no new tests required for docs-only. |
| **Run commands** | `cmake --build <builddir>`, `ctest -R smoke` or label subset. |
| **Evidence** | This audit + JSON counts. |
| **Acceptance** | Gap register counts sum to items; JSON validates; links resolve. |
| **Commit message** | `emu-hw: audit hardware completion state` |

---

## Tranche H1 — Z180 execution correctness

| Field | Content |
| ----- | ------- |
| **Purpose** | Memory, MMU, ROM/RAM, reset/watchdog evidence. |
| **Prerequisites** | H0. |
| **Hardware items** | HW-CPU-001, HW-MMU-001, HW-MEM-001..003, HW-DMA-001, HW-RES-001. |
| **Files** | `millennium_memory.cpp`, `millennium_z180_mmu.cpp`, `millennium_z180_internal.cpp`, tests under `tests/devices/test_z180_*`, `test_memory_*`, `test_stack_*`. |
| **Tests** | `test_z180_mmu_translation`, `test_mmu_register_readback`, `test_memory_map_boot_regions`, `test_stack_ram_mapping`. |
| **Run commands** | MSYS2 build; optional env traces per user mission Phase 5. |
| **Evidence** | `mmu-translation-trace.jsonl`, `memory-trace.jsonl`, `reset-trace.jsonl`. |
| **Acceptance** | MMU + stack facts reproducible in new run folder; RES watchdog promoted from **unknown** only with IC-level evidence or scoped compat item. |
| **Commit message** | `emu-z180: memory map and MMU trace baseline` |

---

## Tranche H2 — Interrupts, timers, RTOS gates

| Field | Content |
| ----- | ------- |
| **Purpose** | IRQ/timer path coherence; no synthetic IRQ floods. |
| **Prerequisites** | H1. |
| **Hardware items** | HW-INT-001, HW-TMR-001. |
| **Files** | Z180 tracing hooks, `millennium_debug.cpp`, interrupt/timer JSONL emitters. |
| **Tests** | `test_z180_itc_il_registers`, `test_z180_timer_registers`, `test_interrupt_vector_loop`, `test_z180_interrupt_trace`. |
| **Evidence** | `interrupt-trace.jsonl`, `timer-trace.jsonl`, correlation notes in `docs/status/`. |
| **Acceptance** | Documented path from timer/ASCI to firmware expectation; EI observed **or** explicit compat-validation if firmware relies on polling. |
| **Commit message** | `emu-z180: interrupt and timer tracing` |

---

## Tranche H3 — ASCI/UART and modem line state

| Field | Content |
| ----- | ------- |
| **Purpose** | Honest UART + modem + host injection; carrier/readback rules. |
| **Prerequisites** | H2 (preferred). |
| **Hardware items** | HW-ASCI-001, HW-MOD-001. |
| **Files** | `millennium_modem*.cpp`, `millennium_hostbridge*.cpp`, `millennium_state.cpp` wiring. |
| **Tests** | `test_uart_transcript`, `test_modem_state_machine`, integration `test_modem_connect`, `test_host_bridge_*`. |
| **Evidence** | `asci-trace.jsonl`, `uart-transcript.log`, modem-related `io-trace.jsonl`. |
| **Acceptance** | Controlled scenario shows DCD/CTS transitions tied to model (no magic bytes per port). |
| **Commit message** | `emu-modem: ASCI modem line state and transcripts` |

---

## Tranche H4 — VFD display hardware

| Field | Content |
| ----- | ------- |
| **Purpose** | Firmware-driven `0x60` / display pipeline; **M6** milestone. |
| **Prerequisites** | H3 recommended (modem/boot gate). |
| **Hardware items** | HW-VFD-001, HW-VFD-002. |
| **Files** | `millennium_vfd*.cpp`, `millennium.lay`, board profiles. |
| **Tests** | `test_vfd_command_decode`, `test_vfd_2line_idle`, `test_vfd_11line_ad`, buffer snapshot tests. |
| **Evidence** | `boot-trace.jsonl` milestone **M6**, `vfd-trace.jsonl`, PNG screenshot. |
| **Acceptance** | Validator passes M6; screenshot matches trace-derived expectation. |
| **Commit message** | `emu-vfd: firmware-driven display path and M6 evidence` |

*(Tranches H5–H14 follow the same schema — full duplicate tables live in git history; summary below.)*

---

## Summary table H5–H14

| Tranche | Focus | Key IDs | Evidence anchor |
| ------- | ----- | ------- | ----------------- |
| **H5** | Keypad, hookswitch, mach PIO, security | HW-KEY-*, HW-SEC-001, HW-MACH-001 | io-trace + keypad tests |
| **H6** | Voiceware uPD7759, ROM banks | HW-VOIC-*, HW-SPK-001, HW-DTMF-001 | voiceware trace + WAV |
| **H7** | Audio routing, telephony decode, call progress | HW-AUD-001, HW-TEL-001, HW-CP-001 | audio-route / telephony JSONL |
| **H8** | Disconnect supervision | HW-SUP-001 | supervision-trace.jsonl |
| **H9** | Card + smartcard | HW-CARD-001, HW-SC-001 | card/swipe traces |
| **H10** | Coin validator | HW-COIN-001 | coin pulse traces |
| **H11** | NVRAM, tables, download | HW-NVRAM-001, HW-MEM-003 | nvram image + checksum tests |
| **H12** | CoinLine Host + NCC/DLOG | HW-HB-001 | host-bridge transcript |
| **H13** | Scenarios + evidence bundles | HW-EV-001 | bundle validator |
| **H14** | CI + release | HW-SCN-001, HW-ART-001 | CI logs + `hardware-final-status.md` |

---

## Run commands (all tranches with firmware proof)

See user mission **Phase 5** PowerShell snippet (`overlay-coinline-driver.ps1`, `build-mame-coinline.ps1`, `run-screenshot-capture.ps1`, `validate-boot-milestones.ps1`). Align env vars with the subsystem under test.
