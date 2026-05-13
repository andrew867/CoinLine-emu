# Test plan — Memory map

## Purpose

Verify that the program-space memory map decoder routes addresses to the correct regions (ROM, RAM, NVRAM, table storage, DLA staging) and respects access modes (R / W / RW).

## Prerequisites

- Built emulator.
- Firmware binary; board profile.
- `fixtures/board/memory-map.json` validated against [`../specs/memory-map.spec.md`](../specs/memory-map.spec.md).

## Fixtures

- `fixtures/board/memory-map.json`
- `fixtures/board/board-profile-2line-vfd.json`
- `fixtures/nvram/factory-default.nvram.json`

## Procedure

1. Run schema validation: `pytest tests/fixtures/test_memory_map_schema.py`.
2. Run layout tests: `pytest tests/fixtures/test_memory_map_layout.py`.
3. Run device-tier tests for region-by-region read/write coverage.
4. Boot firmware and observe RAM init writes during M3.

## Expected behavior

- All declared regions are addressable.
- Read-only regions (ROM) reject writes (logged but discarded).
- Read-write regions accept reads and writes within bounds.
- Out-of-range addresses fall through to the unknown-port-style logger (memory bus equivalent).
- Region overlap test fails the layout validator.

## Pass criteria

- All schema and layout tests pass.
- M3 RAM init observed: writes span the RAM region; SP set inside RAM.

## Fail criteria

- Schema or layout violation.
- ROM accepts a write silently.
- A region not declared but still accessible.

## Evidence artifacts

- M3 entry in `boot-trace.jsonl`.
- `nvram/initial.json`, `nvram/final.json` for NVRAM region exercise.

## Source files touched

- `src/mame/coinline/millennium_memory.cpp`

## Implementation files touched

- `tests/fixtures/test_memory_map_*.py`
- `tests/devices/test_memory_*.cpp`

## Automated test location

- `tests/fixtures/test_memory_map_schema.py`
- `tests/fixtures/test_memory_map_layout.py`
- `tests/devices/test_memory_rom_readonly.cpp`
- `tests/devices/test_memory_ram_rw.cpp`
- `tests/devices/test_memory_nvram_rw.cpp`
- `tests/devices/test_memory_tablestore_bounds.cpp`

## Cross-references

- [`../docs/memory-map.md`](../docs/memory-map.md).
- [`../specs/memory-map.spec.md`](../specs/memory-map.spec.md).
