# Telephony Flash Mitigation Proposal - 2026-05-07

## Purpose

Define implementation options and decision gates to correct CP↔TP emulation fidelity that drives firmware `Telephony board not responding` behavior, while preserving spec conformance and watchdog safety behavior.

This document is design-only and does not authorize code changes.

## Constraints

- Do not weaken actual telephony timeout/reset escalation (`terminal_17`, `terminal_19`).
- Preserve boot/runtime branch semantics (`terminal_00`).
- Keep evidence/reporting integrity for `terminal_20`, `terminal_22`, `terminal_24`, and Terminal-25 outputs.
- Any display heuristic changes must not hide genuine faults.

## Candidate Fix Options

### Option A (Recommended): Restore CP↔TP Runtime Response Cadence Fidelity

Prioritize transport fidelity over UI heuristics:

- Ensure runtime CP poll/health contract receives TP responses with correct cadence and state progression.
- Validate post-handshake progression beyond first `0x72`, `0xC4`, `0xC0` milestones.
- Confirm firmware-observable status continuity during off-hook and craft-entry windows.

Benefits:

- Addresses hardware-emulator core requirement directly.
- Reduces firmware-perceived TP stall conditions at source.

Risks:

- Incorrect cadence tuning could break watchdog/reset semantics.

### Option B: Tighten Not-Responding Display Gate (Secondary)

Change classification from text-only heuristic to a compound gate requiring hard-fault indicators before not-responding display state.

Benefits:

- Prevents transient display false positives after transport is corrected.

Risks:

- If applied first, may mask real TP cadence faults.

### Option C: Acceptance Gate Realignment

Adjust acceptance criteria to avoid false failures when trace shows valid progression:

- Distinguish hard-fail from diagnostic warning.
- Accept hook/keypad success using definitive trace events already present.
- Avoid blanket fail solely because not-responding text appears post-OOS without alarm/timeout evidence.

Benefits:

- Immediate diagnostic fidelity improvement.

Risks:

- If overly permissive, could mask true regressions in CI.

### Option D: Craft Input Timing Envelope Stabilization

Adjust scripted acceptance timing (hook and keypad cadence) to reduce race windows.

Benefits:

- Reduces test flakiness.

Risks:

- Treats symptom, not root cause.

## Risk Matrix

| Option | Effectiveness      | Safety Risk | Implementation Risk | Notes                     |
| ------ | ------------------ | ----------- | ------------------- | ------------------------- |
| A      | High               | Low-Medium  | Medium              | Best root-cause alignment |
| B      | Medium-High        | Medium      | Medium              | Works well with A         |
| C      | High (diagnostics) | Low         | Low-Medium          | Needed regardless of A/B  |
| D      | Low-Medium         | Low         | Low                 | Supportive only           |

## Proposed Combined Strategy

1. Apply Option A first (protocol/cadence fidelity at CP↔TP boundary).
2. Add Option B conservatively as a guardrail, not primary fix.
3. Apply Option C to acceptance scripts/reporting so diagnostics match runtime truth.
4. Use Option D only if residual race remains.

## Verification Protocol (Post-Approval)

1. Replay baseline run profile and compare:
  - Count/duration of not-responding bursts.
  - Presence of true alarm/timeout events.
2. Validate craft sequence:
  - `craft_code_progress` reaches completion.
  - Acceptance A2/A4/A5/A6 reflect trace truth.
3. Regression suite:
  - Telephony watchdog escalation vectors.
  - Boot code selection vectors.
  - User IO timing/fault vectors.
4. Evidence check:
  - Terminal-25 summary fields remain schema-valid.

Success criteria:

- Firmware no longer enters not-responding windows under healthy CP↔TP runtime cadence.
- No regression in true telephony fault signaling.
- Acceptance report aligns with trace-grounded behavior.

## Decision Gates

### Gate 1 - RCA Acceptance

- Approve root cause conclusions in RCA document.

### Gate 2 - Mitigation Selection

- Select A+B+C (and D only if required).

### Gate 3 - Implementation Scope Approval

- Approve specific files/symbols and test updates to modify.

### Gate 4 - Verification Sign-Off

- Confirm all success criteria and no regressions.