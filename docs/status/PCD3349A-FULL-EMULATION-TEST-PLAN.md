# PCD3349A Full Emulation Test Plan

## Purpose

Define executable verification that proves TP behavior is execution-driven and timing-deterministic for CP contract stability.

## Test Stages

### Stage 0: Core CPU Determinism

- Fixed ROM + fixed input stream => identical:
  - PC trace
  - register/PSW trace
  - output byte sequence
- Interrupt entry/return ordering reproducible.

### Stage 1: Port Semantics

- Quasi-bidirectional readback matrix tests:
  - latch high/low vs external high/low combinations.
- Reset mask-option tests for each port.
- T0/T1 branch tests (`JT0/JNT0/JT1/JNT1`) with controlled pins.

### Stage 2: Timer/Interrupt Cadence

- Timer increment and overflow tests against cycle budget.
- External interrupt vector and return behavior tests.
- Timer interrupt pending/clear behavior tests.

### Stage 3: CP Protocol Execution Authority

- CP opcode stimuli must produce TP responses through executed TP path.
- Assert no host-synthesized response bypass in TP backend mode.
- Boot sequence verification:
  - CP query order observed and answered within timing budget.

### Stage 4: Front-Panel/Key/Hook/Line

- Key scan/debounce vectors:
  - stable keydown/keyup, bounce windows, repeat behavior.
- Hook transition debounce vectors.
- Line connect/disconnect and reversal edge vectors.

### Stage 5: Tone Register and Cadence

- HGF/LGF write-only semantics tests.
- `00` disable behavior tests.
- Idle/Stop/Reset tone-state transition tests.
- Call-progress profile cadence vectors for dial/ringback/busy/reorder/SIT/howler classes.

### Stage 6: End-to-End Acceptance

- Craft-entry and runtime watchdog acceptance:
  - sustained run with zero false telephony-not-responding transitions.
- A/B equivalence checkpoints vs legacy backend where expected.

## Required Artifacts

- TP core trace (`pc`, `opcode`, `cycles`, `irq_state`).
- TP port trace (read/write, latch, external value, effective readback).
- TP protocol trace (CP->TP bytes, TP->CP bytes, framing checks).
- TP tone trace (register writes, mode transitions, cadence phase).
- Gate summary JSON with pass/fail and timing deltas.

## Exit Criteria

- All stage gates green.
- No unsupported opcode executed in production vector set.
- No TP backend fake-response or fake-key injection path invoked.
- Runtime watchdog/craft acceptance passes in a single run with deterministic replay.
