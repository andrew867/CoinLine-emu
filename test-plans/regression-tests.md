# Test plan — Regression

## Purpose

Replay golden traces and saved-state checkpoints to ensure that subsequent emulator changes do not regress previously validated behavior.

## Prerequisites

- Built emulator.
- A baseline set of evidence bundles produced from a prior `pass` run of the canonical scenario suite.

## Fixtures

- `fixtures/regression/<bundle_id>/` — frozen evidence bundles + saved states.

## Procedure

1. For each baseline bundle:
   - Reload the same firmware, board profile, NVRAM image.
   - Replay the host-bridge transcript from the baseline.
   - Compare the new evidence bundle with the baseline (modulo timestamps).
2. For each saved-state checkpoint:
   - Load the state.
   - Run a small follow-up scenario that exercises post-state behavior.
   - Confirm the behavior matches the baseline.

## Expected behavior

- Re-running a baseline produces an evidence bundle byte-equal to the baseline (modulo timestamps).

## Pass criteria

- Every baseline replay produces a `pass` status.
- No regression in any subsystem.

## Fail criteria

- Any replay diverges from baseline beyond the timestamp tolerance.

## Evidence artifacts

- Per-bundle diff reports under `out/regression/<bundle_id>/diff.txt`.

## Source files touched

- All emulator source potentially.

## Implementation files touched

- `tests/integration/test_regression_*.cpp`

## Automated test location

- `tests/integration/test_regression_replay.cpp`
- `tests/integration/test_regression_savestate.cpp`

## Cross-references

- [`../docs/test-strategy.md`](../docs/test-strategy.md).
- [`../docs/evidence-bundles.md`](../docs/evidence-bundles.md).
