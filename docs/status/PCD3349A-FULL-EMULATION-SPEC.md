# PCD3349A Full Emulation Specification

## Objective

Provide deterministic, replayable, chip-faithful TP behavior so CP-visible telephony readiness, watchdog, keypad, hook, line-status, and tone behavior are sourced by TP execution semantics rather than host shortcuts.

## Hardware Baseline

- CPU family: MCS-48 class (PCD3349A derivative behavior).
- Program ROM: 4 KiB.
- Internal RAM: 224 bytes.
- I/O: 20 quasi-bidirectional lines.
- Interrupts: single-level vectored external and timer interrupt.
- Timer/event counter: one 8-bit timer/counter.
- Test inputs: T0 and T1.
- Tone block: dual-generator DTMF/call-progress path via derivative registers.
- Clock profile: default `3.579545 MHz` (allow `3.58 MHz` profile alias).

## Normative Behavioral Requirements

### 1. Quasi-Bidirectional Ports

- Model per-port latch state.
- Model external input value.
- Readback behavior must combine latch + external drive rules.
- Provide reset-mask option for initial high/low defaults.
- Preserve per-port metadata for drive policy (quasi/open-drain/push-pull profile annotations).

### 2. Timer/Event Counter 1

- Timer cadence derived from TP cycle counter only.
- No wall-clock sleeping or host callback jitter as authority.
- Expose timer overflow flag/pending behavior for interrupt dispatch.
- Preserve deterministic results under fixed run inputs.

### 3. Interrupt Model

- External IRQ pending, timer IRQ pending.
- Enable/disable state via instruction semantics.
- Single-level priority behavior.
- Deterministic entry and return behavior.
- Trace interrupt entry, vector, and ISR return.

### 4. Reset, Idle, Stop

- Reset clears derivative tone registers and relevant TP state.
- Idle keeps tone generator active.
- Stop halts tone output (unless profile override documents otherwise).
- Emit explicit reset trace marker with cycle count.

### 5. Derivative Tone Register Semantics

- Implement write-only HGF/LGF semantics.
- Register write opcode path only (no chip-visible readback).
- Value `0x00` disables corresponding generator.
- Both generators disabled => tone output off.
- Optional debug shadow is allowed in host traces only.

### 6. CPU State and ISA Coverage

- Implement core CPU state sufficient for ROM execution:
  - A, PSW/carry/flags, register banks, stack, PC, bank flip-flop behavior.
  - timer flag and T0/T1 branch conditions.
- Full ISA coverage target is required for production fidelity.
- During rollout, unsupported opcode path must be explicit and trace-visible.

### 7. T0/T1 Input Semantics

- T0/T1 exposed to branch/test instructions.
- T1 usable as counter input path for event-counter mode.
- Source mapping from telephony board signals must be documented and traceable.

### 8. Clock-Coupled Cadence

- Single TP clock source drives:
  - instruction stepping
  - timer increments
  - tone cadence and phase scheduling
- No mixed authority timing sources.

## CP/TP Contract Requirements

- CP->TP bytes delivered through TP ingress path tied to TP execution model.
- TP responses emitted by executed firmware behavior and port/interrupt contract.
- Watchdog path (`0x38`/`0xC4`) must be deterministic and deadline-safe.
- Boot readiness path must preserve CP init sequence expectations.

## North American Call-Progress Tone Profile

- Dial tone: `350 + 440 Hz` continuous.
- Ringback: `440 + 480 Hz`, `2.0 s on / 4.0 s off`.
- Busy: `480 + 620 Hz`, `0.5 s on / 0.5 s off`.
- Reorder: `480 + 620 Hz`, `0.25 s on / 0.25 s off`.
- SIT: `950`, then `1400`, then `1800 Hz`, about `0.33 s` each, then pause.
- Off-hook/howler profile: four-tone alert class around `1400/2060/2450/2600 Hz` pulsed cadence.

## Conformance Gate

Implementation is considered conformant only when:
- no host-side fake CP reply synthesis remains in TP backend mode,
- no host-side key->opcode direct injection remains in TP backend mode,
- all required traces prove TP cycle-causal behavior for boot and runtime watchdog paths.
