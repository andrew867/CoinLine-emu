# PCD3349A Full Emulation Implementation Plan

## Delivery Model

Work in major tranches with commit checkpoints. Each tranche must include:
- code changes,
- targeted tests,
- trace proof,
- risk notes.

## Tranche 1: Execution Core Foundation

- Replace minimal executor with instruction interpreter covering TP ROM needs.
- Implement:
  - fetch/decode/execute loop
  - RAM/register-bank/PSW/stack/PC model
  - core ALU and control-flow instructions
  - interrupt entry/return path
- Add unsupported-opcode trace and counter.

### Done Criteria
- TP ROM boot loop executes deterministically.
- Core vectors pass.

## Tranche 2: Port + Timer + Interrupt Fidelity

- Implement quasi-bidirectional port model with:
  - latch
  - external input
  - effective readback
- Implement timer/event counter semantics and timer IRQ pending.
- Wire T0/T1 input behavior for branch/counter use.

### Done Criteria
- Port matrix vectors pass.
- Timer/interrupt cadence vectors pass.

## Tranche 3: CP/TP Execution-Driven Protocol

- Remove host opcode-switch synthesis for CP replies in TP mode.
- Feed CP bytes through TP ingress (BUS + external interrupt or equivalent execution path).
- Ensure TP->CP bytes are emitted from executed behavior only.

### Done Criteria
- CP protocol tests prove no host-synthesized bypass.
- Boot and watchdog traces show TP execution causality.

## Tranche 4: Input and Tone Behavioral Fidelity

- Eliminate host direct key->opcode injection in TP mode.
- Implement TP-side key/hook/line scan/debounce as execution-visible input behavior.
- Implement derivative register semantics for HGF/LGF write-only path.
- Implement Idle/Stop/Reset tone behavior.

### Done Criteria
- Front-panel vectors pass with TP execution authority.
- Tone semantics vectors pass.

## Tranche 5: Call-Progress and Acceptance Closure

- Implement call-progress cadence profile tables:
  - dial/ringback/busy/reorder/SIT/howler classes.
- Verify craft/runtime acceptance stability in long-run traces.
- Add conformance report documentation and known limitations list (if any).

### Done Criteria
- Acceptance suite passes end-to-end.
- Conformance report generated with zero critical gaps.

## Risk Controls

- Keep legacy backend switchable for A/B diagnosis.
- Require deterministic seeds and cycle-based validation.
- Block merge on unsupported opcode hits in production vectors.
