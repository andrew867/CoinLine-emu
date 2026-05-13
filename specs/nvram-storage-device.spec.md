# Spec — NVRAM storage device

This spec defines the NVRAM + table-storage device contract. See [`../docs/nvram-and-table-storage.md`](../docs/nvram-and-table-storage.md) for context.

## Purpose

Provide battery-backed storage for configuration, counters, and downloaded tables. Persist across runs.

## Memory regions

Per active board profile:

| Region | Purpose |
| ------ | ------- |
| `nvram` | Configuration, counters. |
| `tablestore` | Downloaded tables. |

## Image format

```jsonc
{
  "version": "1.0",
  "size": "<bytes>",
  "checksum_algorithm": "sum8 | crc16-ibm | none",
  "checksum_value": "0x...",
  "data_b64": "..."
}
```

## State machine

NVRAM is plain memory; "state machine" here means the persistence lifecycle:

```
loaded -> dirty (on write) -> persisted (on save) -> loaded
```

## Persistence rules

- Save on machine close.
- Save on `--checkpoint <path>` if supported.
- Reload from disk on next start unless `--clear-nvram` is supplied.

## Interrupts

None.

## MAME files

- `src/mame/coinline/millennium_nvram.cpp/h`

## Fixture files

- `fixtures/nvram/factory-default.nvram.json`
- `fixtures/nvram/corrupt-checksum.nvram.json`

## Tests

- `tests/devices/test_nvram_persistence.cpp`
- `tests/devices/test_nvram_corrupt_checksum.cpp`
- `tests/devices/test_table_storage_region.cpp`

## Boot milestone dependencies

- M3 (RAM init reads NVRAM).
- M12 (table storage accessed).

## Acceptance criteria

- Image survives reset.
- Corrupt-checksum image triggers the firmware's recovery path.
- Table storage region is bounded; out-of-range writes are logged and rejected.
