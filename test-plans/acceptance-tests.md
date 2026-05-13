# Test plan — Acceptance

## Purpose

Run the full acceptance suite per [`../docs/acceptance-test-plan.md`](../docs/acceptance-test-plan.md). This plan is the rollup; per-area plans contain the details.

## Prerequisites

- Built emulator (Linux + Windows where applicable).
- Firmware binary; canonical board profiles.
- CoinLine Host instance available for host integration gates.

## Fixtures

All `fixtures/scenarios/*.json` and supporting fixtures referenced by them.

## Procedure

Run the gates A through K from [`../docs/acceptance-test-plan.md`](../docs/acceptance-test-plan.md):

- A: Skeleton boots (M0, M1, M2; unknown-port logging).
- B: Z180 core stable (MMU, ASCI, PRT, INT, DMA).
- C: VFD (M6, idle snapshot 2-line + 11-line).
- D: Front-panel inputs (keypad, hookswitch, security).
- E: Modem + host bridge (M8, modem state machine, loopback).
- F: NVRAM + tables (persistence, corrupt-checksum, table storage region).
- G: Card and coin (magstripe accept/reject, smartcard ATR, coin pulses, coin errors, alerter).
- H: Boot to idle (M10, service mode entry).
- I: CoinLine Host integration (M11, M12, table download, DLOG submit).
- J: Scenarios + evidence (suite green; bundle complete).

## Expected behavior

All gates green per [`../docs/acceptance-test-plan.md`](../docs/acceptance-test-plan.md).

## Pass criteria

- Every gate green on the canonical CI matrix.
- Evidence bundles attached to PR.

## Fail criteria

- Any gate red.

## Evidence artifacts

- One evidence bundle per scenario in the suite.
- CI run URL.

## Source files touched

- All emulator source.

## Implementation files touched

- All test directories.

## Automated test location

- `tests/integration/test_acceptance_*.cpp` and the underlying per-area tests.

## Cross-references

- [`../docs/acceptance-test-plan.md`](../docs/acceptance-test-plan.md).
- [`../docs/ci-and-release.md`](../docs/ci-and-release.md).
