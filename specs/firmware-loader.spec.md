# Spec — Firmware loader

This spec defines the firmware loader contract for `coinline-emu`. It is normative for the loader code path that brings `../firmware/flash.bin` into the ROM region.

## Inputs

| Input | Source |
| ----- | ------ |
| Firmware binary path | `-firmware <path>` CLI flag (`../firmware/flash.bin`). |
| Expected hashes | `fixtures/firmware/firmware-hashes.json` (SHA-256 keyed by firmware version). |
| Board profile | `-board <path>`; provides `memory.rom_size`. |

## Outputs

| Output | Form |
| ------ | ---- |
| ROM region populated | Bytes loaded at base `0x00000` of the program-space ROM region. |
| Loader log entry | `{ "milestone": "M0", "ts": "...", "sha256": "...", "size": ... }`. |
| Hash assertion | If a matching hash is present in `firmware-hashes.json`, asserted equal; if not, logged as `firmware_unknown_hash`. |

## Behavior

| Step | Behavior |
| ---- | -------- |
| Read binary | The loader reads the file specified by `-firmware`. |
| Validate size | Size must match `memory.rom_size` in the board profile. Mismatch → fatal error. |
| Compute SHA-256 | Computed from the byte stream. |
| Lookup hash | If `firmware-hashes.json` is present, the SHA-256 must match an entry; mismatch → fatal error. |
| Populate ROM | Bytes copied into the ROM region. |
| Emit M0 | Loader log entry produced. |

## Failure modes

| Failure | Action |
| ------- | ------ |
| File not found | Fatal; emulator exits with non-zero code and a clear message. |
| Size mismatch | Fatal; clear message naming both expected and actual sizes. |
| Hash mismatch | Fatal if `firmware-hashes.json` is strict-mode enabled; warning otherwise. |
| File too short / read error | Fatal; partial ROM is never accepted. |

## Strict mode

Strict mode is enabled when the board profile sets `firmware.strict_hash = true`. CI runs strict mode. Local development may run permissive mode for early bring-up.

## Tests

- `tests/boot/test_firmware_load.cpp` — covers all behaviors above.
- `tests/boot/test_firmware_hash_mismatch.cpp` — verifies fatal exit on mismatch in strict mode.
- `tests/boot/test_firmware_size_mismatch.cpp` — verifies fatal exit on size mismatch.

## Cross-references

- [`../docs/boot-milestones.md`](../docs/boot-milestones.md).
- [`../docs/memory-map.md`](../docs/memory-map.md).
- [`../docs/board-profiles.md`](../docs/board-profiles.md).
