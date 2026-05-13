# Test plan — Host integration

## Purpose

Verify that the emulated terminal completes representative end-to-end flows against a real running CoinLine Host instance, with no source-level coupling between trees.

## Prerequisites

- Built emulator.
- Firmware; board profile.
- CoinLine Host instance reachable per [`../docs/host-integration-plan.md`](../docs/host-integration-plan.md).

## Fixtures

- `fixtures/scenarios/modem-connect.json`
- `fixtures/scenarios/table-download.json`
- `fixtures/scenarios/card-call.json`
- `fixtures/scenarios/coin-call.json`

## Procedure

1. Start CoinLine Host (and its modem-leg gateway).
2. Run each scenario in sequence with a fresh evidence directory.
3. Inspect CoinLine Host's request log / DLOG to confirm the emulator's traffic was understood.
4. Reset the emulator between scenarios.

## Expected behavior

- Modem connect reaches M11.
- Table download reaches M12 with persistence.
- Card call produces a DLOG entry visible to CoinLine Host.
- Coin call produces a DLOG entry visible to CoinLine Host.

## Pass criteria

- All scenarios pass.
- Evidence bundles capture the host-bridge transcripts.

## Fail criteria

- Any scenario times out.
- Bytes diverge between sides.
- DLOG entries missing or malformed.

## Evidence artifacts

- `host-bridge/transcript.jsonl` per scenario.
- `nvram/final.json`.
- CoinLine Host request log (collected separately).

## Source files touched

- `src/mame/coinline/millennium_hostbridge.cpp/h`

## Implementation files touched

- `tests/integration/test_host_*.cpp`

## Automated test location

- `tests/integration/test_modem_connect.cpp`
- `tests/integration/test_table_download_e2e.cpp`
- `tests/integration/test_card_call.cpp`
- `tests/integration/test_coin_call.cpp`
- `tests/integration/test_dlog_submit_e2e.cpp`

## Cross-references

- [`../docs/host-integration-plan.md`](../docs/host-integration-plan.md).
