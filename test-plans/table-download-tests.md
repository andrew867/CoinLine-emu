# Test plan — Table download

## Purpose

Verify the firmware-driven table download flow against CoinLine Host and the persistence of downloaded tables in NVRAM/table storage.

## Prerequisites

- Built emulator.
- Firmware; board profile.
- CoinLine Host instance running.

## Fixtures

- `fixtures/scenarios/table-download.json`
- `fixtures/nvram/factory-default.nvram.json`
- A staged table payload published by CoinLine Host.

## Procedure

1. Start CoinLine Host with a known table to be downloaded.
2. Boot the emulator with the host bridge pointing at CoinLine Host.
3. Run `table-download.json`.
4. After the scenario completes, reset the emulator and observe that the downloaded table remains in NVRAM/table storage.

## Expected behavior

- M11 reached when firmware initiates the host call.
- Table-storage writes appear in `nvram/diff.jsonl`.
- M12 reached when firmware confirms storage.
- Post-reset boot reads the persisted table.

## Pass criteria

- Scenario completes without timeouts.
- Table is observable in `nvram/final.json` and persists across reset.
- DLOG entry (if applicable) appears at CoinLine Host endpoint.

## Fail criteria

- Carrier loss during the test (without injection).
- Table not persisted.
- Wrong table id committed.

## Evidence artifacts

- `host-bridge/transcript.jsonl`.
- `nvram/diff.jsonl`, `nvram/final.json`.
- `boot-trace.jsonl` of post-reset boot.

## Source files touched

- `src/mame/coinline/millennium_nvram.cpp/h`
- `src/mame/coinline/millennium_modem.cpp/h`
- `src/mame/coinline/millennium_hostbridge.cpp/h`

## Implementation files touched

- `tests/integration/test_table_download_*.cpp`

## Automated test location

- `tests/integration/test_table_download_e2e.cpp`
- `tests/integration/test_table_download_carrier_loss.cpp`

## Cross-references

- [`../docs/table-download-behavior.md`](../docs/table-download-behavior.md).
- [`../docs/host-integration-plan.md`](../docs/host-integration-plan.md).
