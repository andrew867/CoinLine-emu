# NVRAM and table storage

This document describes the NVRAM device and table storage region. NVRAM holds configuration and counters; table storage holds downloaded tables. Both are persisted across runs and emulated by the same device class with separate sub-regions.

## Purpose

- Provide battery-backed storage that survives reset.
- Verify checksums and recover from corruption per firmware behavior.
- Support factory-default and corrupt-checksum images for test fixtures.
- Persist downloaded tables coming from CoinLine Host.

## Memory regions

Per the active board profile (`memory.nvram_*`, `memory.table_storage_*`):

| Region | Purpose | Persistence |
| ------ | ------- | ----------- |
| `nvram` | Configuration records, counters, audit-relevant flags | persisted to `nvram/` |
| `tablestore` | Downloaded tables | persisted to `nvram/` |

Both regions are part of the program-space memory map and accessible to firmware via normal load/store instructions.

## NVRAM image format

Internal layout is firmware-specific and pinned in the firmware evidence inventory. Public format is the JSON envelope used by fixtures:

```jsonc
{
  "version": "1.0",
  "size": 8192,
  "checksum_algorithm": "sum8 | crc16-ibm | none",
  "checksum_value": "0x...",
  "data_b64": "..."
}
```

## Default and corrupt fixtures

| Fixture | Purpose |
| ------- | ------- |
| `fixtures/nvram/factory-default.nvram.json` | Pristine NVRAM image used at first boot or after `--clear-nvram`. |
| `fixtures/nvram/corrupt-checksum.nvram.json` | Same data with intentionally wrong checksum to exercise the firmware's recovery path. |

## Persistence

- The MAME track uses MAME's NVRAM machinery to save/restore on machine close.
- The MIT-clean track persists to a JSON file under `nvram/`.
- Both tracks honor the operator's `-nvram <path>` to load a specific image.

## Tests

- `tests/devices/test_nvram_persistence.cpp` — round-trip across reset.
- `tests/devices/test_nvram_corrupt_checksum.cpp` — corrupt fixture triggers recovery path.
- `tests/devices/test_table_storage_region.cpp` — writes are bounded and persisted.
- `tests/integration/test_table_download_e2e.cpp` — downloaded table appears in table storage.

## Boot-milestone dependencies

- M3 (RAM init) — the firmware accesses NVRAM during initialization.
- M12 — table storage accessed during table download / upload.

## Acceptance criteria

- NVRAM image survives reset.
- Corrupt-checksum image triggers the firmware's expected recovery path.
- Table storage region writes are bounded and persisted.
- The factory-default image produces a deterministic boot to idle.

## Cross-references

- [`../specs/nvram-storage-device.spec.md`](../specs/nvram-storage-device.spec.md).
- [`../test-plans/nvram-storage-tests.md`](../test-plans/nvram-storage-tests.md).
- [`table-download-behavior.md`](table-download-behavior.md).
- [`firmware-download-storage.md`](firmware-download-storage.md).
