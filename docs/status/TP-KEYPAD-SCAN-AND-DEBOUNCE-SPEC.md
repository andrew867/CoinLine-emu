# TP Keypad Scan And Debounce Spec

## Scope

- Specify TP-side keypad and hook scanning behavior for Rev2 target.

## Sampling Inputs

- `KEYMATRIX`: primary keypad and hook bit.
- `TERMINAL21_SOFTKEYS`: optional 11-line softkeys.
- `LINECTRL`: line supervision pulse sources.

## Debounce Rules

- Softkeys:
  - 25 ms stable window before state acceptance in 11-line profile.
- Hook:
  - 40 ms transition guard to drop bounce-like contradictory edges.
- Key release:
  - synthesize `KEY_RELEASE (0x5E)` after 3000 ms if release missing for dial/rep keys.

## Event Mapping

- Dial keys emit TP opcodes (`0x20..0x3E` subset).
- Rep-dial keys emit `0x40..0x52` even opcodes.
- Hook transitions emit:
  - transition byte (`0x60`/`0x62`)
  - state byte (`0x6C`/`0x6E`)
- Line supervision emits:
  - connection/interruption (`0x66`/`0x64`)
  - reversal pulses (`0x68`/`0x6A`) unless gated.

## Abuse/Fault Guards

- Duplicate release suppression.
- Illegal multi-softkey sample tracking.
- Rapid hook transition escalation path.

## Trace Proof Requirements

- Every accepted key edge must be visible in:
  - TP input edge trace
  - TP queued-event trace
  - CP-consumed byte trace