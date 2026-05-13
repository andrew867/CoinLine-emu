# TP Port Wiring Spec

## Intent

- Define TP-facing logical ports and their mapping to emulator input sources.
- Separate Rev2 candidate wiring from Rev1 reference-only wiring.

## Logical TP Inputs

- `tp_keymatrix`:
  - source: `KEYMATRIX` input port
  - includes numeric keys, special keys, rep-dial keys, hook bit
- `tp_linectrl`:
  - source: `LINECTRL` input port
  - line connected/interruption and reversal pulses
- `tp_softkeys`:
  - source: `TERMINAL21_SOFTKEYS` input port
  - active only for 11-line profile
- `tp_security_gate`:
  - source: `SECMASK` input port
  - lock/door/vault/service gating context

## Rev2 Candidate Wiring (active target)

- High-confidence mapping:
  - keypad path enters TP via local digital mux (`U5` candidate, `74HC151`)
  - hook and line supervision transitions are TP-observable and serialized as TP->CP opcodes
- Medium-confidence mapping:
  - analog mux (`U16` candidate, `74HC4051`) influences local audio route selection
  - forgotten-card path may be in-line via alerter->hookswitch->keypad->TP

## Rev1 Reference Wiring (non-authoritative)

- Keep as compatibility profile only:
  - daughter-board reroute for smart-card alert/forgotten-card behavior
  - not assumed as Rev2 default

## Output Mapping

- TP->CP single-byte events:
  - dial pad, softkey, hook state, line state opcodes
- TP->CP framed responses:
  - readiness/status/error-report frames (`C0/C4/C2` class)

## Validation Hooks

- Confirm byte emission latency from input edges in traces:
  - `tp-keypad-input-trace.jsonl`
  - `tp-cp-keypad-protocol-trace.jsonl`
  - `tp-readiness-sequence-trace.jsonl`