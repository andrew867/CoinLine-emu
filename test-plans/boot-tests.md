# Test plan — Boot

## Purpose

Verify that the supported terminal firmware loads, executes its reset vector, and reaches each declared boot milestone (M0–M12) deterministically.

## Prerequisites

- Built emulator binary per [`../BUILDING.md`](../BUILDING.md).
- Firmware binary at `../firmware/flash.bin`; SHA-256 matches a row in `fixtures/firmware/firmware-hashes.json`.
- Board profile `fixtures/board/board-profile-2line-vfd.json`.
- Factory NVRAM image `fixtures/nvram/factory-default.nvram.json`.

## Fixtures

- `fixtures/board/board-profile-2line-vfd.json`
- `fixtures/board/memory-map.json`, `io-port-map.json`, `interrupt-map.json`, `device-map.json`
- `fixtures/nvram/factory-default.nvram.json`
- `fixtures/scenarios/boot-to-idle.json`

## Procedure

1. Launch the emulator with the firmware, board profile, and factory NVRAM.
2. Run `fixtures/scenarios/boot-to-idle.json`.
3. Capture the evidence bundle.
4. Compare the boot trace against the milestone ladder in [`../docs/boot-milestones.md`](../docs/boot-milestones.md).

## Expected behavior

- M0 logged: firmware SHA-256 + size match expectations.
- M1–M4 reached within `timeout_cycles` per milestone.
- M5–M9 reached as devices come online (subject to tranche progress).
- M10 reached: VFD buffer matches `fixtures/display/vfd-2line-idle.json`.

## Pass criteria

- Every milestone in scope for the active tranche is reached within its `timeout_cycles`.
- Evidence bundle is complete per [`../specs/evidence-bundle.spec.md`](../specs/evidence-bundle.spec.md).
- `scenario_result.json.status == "pass"`.

## Fail criteria

- Any in-scope milestone not reached.
- Boot trace contains an `error` entry.
- Evidence bundle missing required artifacts.

## Evidence artifacts

- `out/boot-to-idle/manifest.json`
- `out/boot-to-idle/boot-trace.jsonl`
- `out/boot-to-idle/io-trace.jsonl`
- `out/boot-to-idle/vfd/final.json`

## Source files touched

- `src/mame/coinline/millennium.cpp`, `millennium_state.cpp`, `millennium_memory.cpp`, `millennium_io.cpp`

## Implementation files touched

- All `tests/boot/*.cpp`.

## Automated test location

- `tests/boot/test_reset_vector.cpp`
- `tests/boot/test_first_instructions.cpp`
- `tests/boot/test_ram_init.cpp`
- `tests/boot/test_rtos_entry.cpp`
- `tests/integration/test_boot_to_idle.cpp`

## Cross-references

- [`../docs/boot-milestones.md`](../docs/boot-milestones.md).
- [`../docs/architecture.md`](../docs/architecture.md).
