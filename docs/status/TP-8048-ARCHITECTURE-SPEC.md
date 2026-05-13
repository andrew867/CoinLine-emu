# TP 8048 Architecture Spec

## Scope

- Define the telephony processor (TP) backend architecture that supports A/B selection between:
  - legacy TP behavior in `millennium_state`
  - new `millennium_pcd3349a` wrapper backed by `millennium_am8048_core`
- Preserve existing CP-visible behavior while enabling chip-oriented modeling for keypad, hook, line status, and tone state.

## Target Components

- `millennium_am8048_core`:
  - generic AM8048/MCS-48 execution core abstraction
  - cycle counter and deterministic stepping API
  - port-latch read/write callbacks
- `millennium_pcd3349a`:
  - TP chip wrapper that owns:
    - CP command handling
    - TP response framing
    - front-panel scan/debounce interpretation
    - hook and line supervision event mapping
    - tone-mode intent output (dial tone/NIS/none)
- `millennium_state`:
  - backend selector and bridge to existing MAME devices
  - feeds KEYMATRIX/LINECTRL/SOFTKEY inputs to selected TP backend
  - consumes TP output bytes into CSI/O receive queue

## Backend Selection Contract

- Compile-time selector:
  - `COINLINE_ENABLE_TP8048_BACKEND` (default enabled for this target)
- Runtime/trace metadata:
  - emit selected backend id (`legacy` or `pcd3349a`) in TP readiness/runtime traces

## Data Flow

1. CP transmits a TP opcode over CSI/O.
2. Selected backend parses opcode and updates TP-internal state.
3. Backend emits zero or more TP->CP bytes/frames.
4. `millennium_state` enqueues TP bytes on CSI/O receive queue.
5. CP firmware consumes bytes and advances telephony/craft state.

## Determinism Requirements

- All TP event ordering must be deterministic from:
  - CP opcode sequence
  - sampled KEYMATRIX/LINECTRL/SOFTKEY snapshots
  - cycle-driven poll cadence
- No synthetic VFD text insertion.
- No direct CP memory patching.

## Non-Goals (Phase 1)

- Full undocumented PCD3349A microcode reconstruction.
- Full analog audio chain simulation.
- Bit-perfect mixed-signal modeling of modem/line circuitry.

