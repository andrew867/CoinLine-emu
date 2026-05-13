# Test plan — NVRAM storage

## Purpose

Verify NVRAM persistence across reset, corrupt-checksum recovery, and table-storage region bounds.

## Prerequisites

- Built emulator.
- Firmware; board profile.
- `fixtures/nvram/factory-default.nvram.json` and `corrupt-checksum.nvram.json`.

## Fixtures

- `fixtures/nvram/factory-default.nvram.json`
- `fixtures/nvram/corrupt-checksum.nvram.json`

## Procedure

1. Boot with factory image; observe firmware reaches idle.
2. Cause firmware to write a known-value to NVRAM (e.g., via service-mode operation).
3. Reset; observe value persists.
4. Boot with corrupt-checksum image; observe firmware enters expected recovery path.
5. Run table-storage region bounds test (writes at base, base+size-1 succeed; base+size fails).

## Expected behavior

- Factory image yields deterministic boot to idle.
- NVRAM diff after a known mutation matches expected.
- Corrupt image triggers firmware recovery (visible on VFD).
- Out-of-range table-storage writes are rejected with a logged event.

## Pass criteria

- All sub-tests pass.
- NVRAM image survives reset.

## Fail criteria

- Mutation not persisted.
- Corrupt image accepted silently.
- Out-of-range write accepted.

## Evidence artifacts

- `nvram/initial.json`, `nvram/final.json`, `nvram/diff.jsonl`.
- `vfd/snapshots/*.json` (recovery message).

## Source files touched

- `src/mame/coinline/millennium_nvram.cpp/h`

## Implementation files touched

- `tests/devices/test_nvram_*.cpp`

## Automated test location

- `tests/devices/test_nvram_persistence.cpp`
- `tests/devices/test_nvram_corrupt_checksum.cpp`
- `tests/devices/test_table_storage_region.cpp`

## Cross-references

- [`../docs/nvram-and-table-storage.md`](../docs/nvram-and-table-storage.md), [`../specs/nvram-storage-device.spec.md`](../specs/nvram-storage-device.spec.md).
