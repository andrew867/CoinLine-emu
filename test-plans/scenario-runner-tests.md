# Test plan — Scenario runner

## Purpose

Verify that the scenario runner correctly interprets every declared verb, fails on assertion mismatch, and produces a complete evidence bundle on every run.

## Prerequisites

- Built emulator.
- Firmware; board profile.

## Fixtures

- All `fixtures/scenarios/*.json`.

## Procedure

1. Validate every scenario file against [`../specs/scenario-runner.spec.md`](../specs/scenario-runner.spec.md).
2. Run each scenario and confirm it produces the expected evidence bundle.
3. Run a deliberately failing scenario (a copy of `boot-to-idle.json` with an impossible `expect_vfd_text`) and confirm:
    - The scenario fails on the assertion.
    - The evidence bundle is still produced with `status = "fail"` and a populated `fail_reason`.

## Expected behavior

- Every verb dispatches correctly.
- Assertions enforce equality byte-for-byte / cycle-for-cycle.
- Timeouts are reported with the elapsed cycles.

## Pass criteria

- All scenario schemas validate.
- All passing scenarios produce `status = "pass"`.
- The failing scenario produces `status = "fail"` with evidence.

## Fail criteria

- A verb is silently ignored.
- An assertion mismatch produces `status = "pass"`.
- A failing run does not produce evidence.

## Evidence artifacts

- One evidence bundle per scenario.

## Source files touched

- Scenario runner under `tools/` and `src/mame/coinline/millennium_debug.cpp`.

## Implementation files touched

- `tests/integration/test_scenario_*.cpp`

## Automated test location

- `tests/fixtures/test_scenario_schema.py`
- `tests/integration/test_scenario_runner_smoke.cpp`
- `tests/integration/test_scenario_runner_failure.cpp`

## Cross-references

- [`../docs/scenario-runner.md`](../docs/scenario-runner.md).
- [`../specs/scenario-runner.spec.md`](../specs/scenario-runner.spec.md).
