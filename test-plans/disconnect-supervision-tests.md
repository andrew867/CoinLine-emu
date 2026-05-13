# Test plan — Disconnect supervision

## Proof strategy

| Proof tier | Requirement |
| ---------- | ----------- |
| **A — ROM execution** | ROM reaches rated-call / establish states; inject/receive supervision via modeled telephony RX until hang-up — timer latency measured from emulator cycle counter. |
| **B — Scripted supervision replay** | Deterministic byte/time fixture verified against hardware-validation capture (same hang-up latency within tolerance). |

## Preconditions

- Cutoff scalar loaded from profile; timer tick base configurable.
- ADSS-equivalent state exposed in trace for assertions.

## Cases

| ID | Name | Steps | Pass criteria |
| -- | ---- | ----- | ------------- |
| DS-01 | Supervised establish | Tier A: reversal establish path **or** Tier B: replay | Establish shadow + wink timer armed |
| DS-02 | Post-wink reversal disconnect | After wink expiry inject `0x68`/`0x6A` | Teardown event |
| DS-03 | Interruption → cutoff | `0x64` then no `0x66` until scalar expires | Hang-up at **T_cutoff** ± tolerance |
| DS-04 | Interruption cancel | `0x64` then `0x66` before expiry | Timer cancelled; no hang-up |
| DS-05 | Grace auxiliary | Profile `grace_before_collect`; inject second reversal in window | Disconnect / collect abort per profile |
| DS-06 | Auxiliary absent | Same without grace flag | Baseline (no grace branch) |
| DS-07 | Composite CO status | Inject `0x80`–`0x86` per profile bit decode | Parsed bits match expectation |
| DS-08 | Upload interaction | Disconnect during staged upload session | Host-visible policy trace (reference upload docs) |

## Measurement

| Quantity | Method |
| -------- | ------ |
| Cutoff duration | Emulator cycles → ms using CPU clock + tick profile |
| Ground truth | Oscilloscope measurement on hardware validation terminal |

## Fixtures

- `fixtures/board/disconnect-supervision-map.json`
- Hardware-validation supervision timeline (**`compatibility_validation_required`**)

## Acceptance

- Any timer-based case **without** documented scalar→ms mapping must be flagged **`compatibility_validation_required`** in test output.
