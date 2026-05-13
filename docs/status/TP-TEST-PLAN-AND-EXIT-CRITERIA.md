# TP Test Plan And Exit Criteria

## Stage Plan

- Stage 1: Legacy vs new backend parity on boot and basic telephony readiness.
- Stage 1b: TP hardware inference checks (keypad/hook/line event causality).
- Stage 1c: PCP timing/persistence checks (poll cadence, timeout progression, mode-dependent memory behavior).
- Stage 2: Runtime stability (no post-OOS ready dropout).
- Stage 3: Craft-only A6 closure with truth-chain evidence.
- Stage 4: Full acceptance A1-A6 all true in one run.

## Test Entry Conditions

- Build succeeds with selected backend.
- Required trace sinks configured.
- Craft-only runner and validator scripts available.

## Evidence Artifacts

- `craft-entry-summary.json`
- `craft-truth-chain.jsonl`
- telephony/runtime/keypad/CP protocol traces referenced by validator

## Exit Criteria

- A6 pass in craft-only run with all chain links true.
- Full acceptance A1-A6 true in the same run.
- No fake/injected behavior paths.
- Legacy backend remains selectable and functional for A/B comparison.