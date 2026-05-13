# Test plan — Firmware download (DLA)

## Purpose

Verify the firmware download / DLA path: staging writes to the DLA region, apply, and reboot.

## Prerequisites

- Built emulator.
- Two firmware binaries (current + replacement) with matching `firmware-hashes.json` rows.
- CoinLine Host instance running with firmware-package management configured (or a simulated host via the host bridge).

## Fixtures

- `fixtures/board/board-profile-2line-vfd.json`
- A test firmware binary used as the staging payload (provided by the operator; not committed).

## Procedure

1. Boot with the current firmware.
2. Initiate a DLA from CoinLine Host (or via the host bridge directly).
3. Observe staging writes appear in the DLA region.
4. On apply, observe ROM region updates.
5. Reset; observe firmware boots from the new image.

## Expected behavior

- Staging writes land at `dla_stage_base..+dla_stage_size`.
- Apply triggers a reset and the new ROM is loaded on next boot.
- Interrupted download leaves NVRAM in a coherent state.

## Pass criteria

- Staged image equals the supplied firmware byte-for-byte.
- After reset, the boot trace's `firmware.sha256` matches the new firmware's hash.

## Fail criteria

- Staging writes fall outside the DLA region.
- Apply does not trigger reset.
- ROM region not updated.

## Evidence artifacts

- `nvram/diff.jsonl` showing staging writes.
- `boot-trace.jsonl` from the post-apply reboot.

## Source files touched

- `src/mame/coinline/millennium_nvram.cpp/h`
- `src/mame/coinline/millennium_state.cpp/h`

## Implementation files touched

- `tests/devices/test_firmware_download_*.cpp`
- `tests/integration/test_firmware_download_e2e.cpp`

## Automated test location

- `tests/devices/test_firmware_download_staging.cpp`
- `tests/devices/test_firmware_download_apply.cpp`
- `tests/integration/test_firmware_download_e2e.cpp`

## Cross-references

- [`../docs/firmware-download-storage.md`](../docs/firmware-download-storage.md).
- [`../docs/host-integration-plan.md`](../docs/host-integration-plan.md).
