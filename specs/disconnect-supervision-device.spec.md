# Spec — Disconnect supervision device

## Purpose

Translate supervision **status bytes** into hang-up / billing finalization signals.

## Supervision inputs

| Code | Hex | Maps to internal class |
| ---- | --- | ---------------------- |
| `LINE_INTERRUPTION` | `0x64` | Interruption |
| `LINE_CONNECTION` | `0x66` | Connection |
| `LINE_REVERSAL_0` | `0x68` | Reversal/polarity event |
| `LINE_REVERSAL_1` | `0x6A` | Reversal/polarity event |
| `CO_LINE_STATUS_1`–`4` | `0x80`–`0x86` | Composite CO encoding — **decode per profile** |

## Auxiliary board model

Optional behaviors (**all `compatibility_validation_required`** until schematic-linked):

| Behavior | Trigger |
| -------- | ------- |
| Grace-before-collect | Supervision reversal during grace window |
| OPR / reroute spill | Interruption clears pending collect timers |
| Relay exclusivity | Single appliance on CO pair — contention via relay matrix |

## Line event simulation

Scenario verbs inject `{code, t_ms}` tuples into telephony decode queue.

## Disconnect event timing

Cutoff timer starts on interruption classes per ADSS-equivalent state; cancels on connection.

Default ROM scalar **`45`** — map to milliseconds per profile validation.

## Call finalization event

Raise `call_teardown` after supervisor confirms disconnect path; consumers close CDR staging.

## Tests

[`../test-plans/disconnect-supervision-tests.md`](../test-plans/disconnect-supervision-tests.md).

## Acceptance criteria

1. Timer restart/cancel semantics match golden supervision traces.
2. Supervised established call disconnects on reversal after wink window.
