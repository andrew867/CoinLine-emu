# Umbrella spec: Z180 CPU, MMU, interrupts, timers, DMA, reset

Hardware IDs: **HW-CPU-001**, **HW-MMU-001**, **HW-INT-001**, **HW-TMR-001**, **HW-DMA-001**, **HW-RES-001**

## HW-CPU-001 — Z180 core

| Field | Detail |
| ----- | ------ |
| **Purpose** | Execute firmware source tree firmware with correct instruction set and timing baseline. |
| **Hardware-facing** | External bus, clocks — delegated to MAME `z80180_device`. |
| **Firmware-facing** | Standard Z180 program-visible state; internal peripherals via ICR base. |
| **Reset state** | Per Zilog/Z180 documentation; MAME device handles. |
| **MAME sources** | Upstream `src/devices/cpu/z180/`; machine `millennium.cpp`. |
| **Trace events** | Optional extended traces — not gate for milestone proof. |
| **Evidence** | Firmware boots through MMU setup; PC/SP traces in debug snapshots. |
| **Tests** | `tests/devices/test_z180_*.cpp`, boot tests. |
| **Unknowns** | None for core ISA — timing cycle-accuracy vs silicon is global MAME scope. |
| **Implementation acceptance** | Core unchanged in overlay; machine uses official CPU device. |
| **Field validation** | Optional ICE comparison — not required for **complete_verified** core. |

## HW-MMU-001 — MMU

| Field | Detail |
| ----- | ------ |
| **Purpose** | Logical↔physical translation for ROM/RAM as firmware configures CBR/BBR/CBAR. |
| **Ports/registers** | Z180 internal MMU registers (ICR-relative). |
| **Memory regions** | See `docs/memory-map.md`, `millennium_memory.cpp`. |
| **Read/write** | Delegated to Z180; overlay maps flash/RAM regions. |
| **Trace events** | `mmu-translation-trace.jsonl` when env enabled. |
| **Evidence** | `boot-milestone-status.json`: translation active; stack mapping test. |
| **Tests** | `test_z180_mmu_translation.cpp`, `test_mmu_register_readback.cpp`. |
| **Unknowns** | Corner-case bank combos vs rare firmware paths — promote via unknown-port + traces. |
| **Implementation acceptance** | Tests pass; MMU trace shows coherent logical/physical for sampled PCs. |
| **Field validation** | Compare to trace-backed memory model / hardware probe if disputed. |

## HW-INT-001 — Interrupt controller / ITC / IL

| Field | Detail |
| ----- | ------ |
| **Purpose** | Deliver maskable interrupts consistent with Z180 + firmware enables. |
| **Behavior** | IM0/IM1/IM2, IFF1/IFF2, ITC/IL register semantics — see `docs/interrupt-map.md`. |
| **Trace events** | `interrupt-trace.jsonl` (sampled). |
| **Evidence** | Current runs show `iff1=false` / `im=0` in window — **EI path not proven** for full RTOS tick. |
| **Tests** | `test_z180_itc_il_registers.cpp`, `test_interrupt_vector_loop.cpp`, `test_z180_interrupt_trace.cpp`. |
| **Unknowns** | Whether firmware enables IRQs before or after display init — **compat-validation** until traces prove. |
| **Implementation acceptance** | No fabricated IRQ storms; trace reflects CPU state. |
| **Field validation** | Logic analyzer on `/INT` vs emulator — optional. |

## HW-TMR-001 — PRT / timers

| Field | Detail |
| ----- | ------ |
| **Purpose** | Programmable reload timers for RTOS tick and UART baud-related references. |
| **Trace** | `timer-trace.jsonl` when enabled. |
| **Tests** | `test_z180_timer_registers.cpp`, `test_z180_prt.cpp`. |
| **Unknowns** | Exact tick rate vs crystal tolerance — measure or document tolerance. |
| **Implementation acceptance** | Register R/W matches expected masking; no fake timer IRQ injection. |

## HW-DMA-001 — DMA

| Field | Detail |
| ----- | ------ |
| **Purpose** | Z180 DMA channels if firmware uses them. |
| **Status** | **complete_unverified** — DMA exists in core; terminal use **not** proven in audit. |
| **Tests** | `test_z180_dma.cpp`. |
| **Unknowns** | Search firmware source tree for DMA channel enables; until found, treat as optional. |

## HW-RES-001 — Reset / watchdog

| Field | Detail |
| ----- | ------ |
| **Purpose** | Power-on reset; watchdog timeout if external IC exists. |
| **Trace** | `reset-trace.jsonl` when `COINLINE_TRACE_RESET` set. |
| **Status** | **unknown** — supervisor IC timing not sourced. |
| **Implementation acceptance** | Document assumptions; add compat-validation for timeout behavior. |
| **Field validation** | Bench capture of reset pulse width / WDTO. |

