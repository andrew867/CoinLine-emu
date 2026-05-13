# Firmware download (DLA) staging

This document describes the firmware-download (DLA / "Download All") staging region used when CoinLine Host pushes a new firmware image to the terminal.

## Purpose

- Receive the staged firmware image into a dedicated region (`dlastage`).
- Maintain checksum / progress state during transfer.
- Trigger firmware's apply-and-reboot path on completion.

## Memory region

Per the active board profile (`memory.dla_stage_base`, `memory.dla_stage_size`).

The staging region is **separate** from the active ROM region. The firmware copies from staging to flash on apply, which is modeled here as a region transition (the emulator updates the ROM region from the staging region on the firmware's apply command).

## Behavior

| Step | Behavior |
| ---- | -------- |
| Receive bytes | Firmware writes received bytes to the staging region. |
| Verify | Firmware computes a checksum / signature. |
| Apply | Firmware copies staging into the ROM region (modeled by the emulator). |
| Reboot | Firmware triggers a reset; the emulator restarts with the new ROM contents. |

## Tests

- `tests/devices/test_firmware_download_staging.cpp` — verifies staging writes land in the DLA region.
- `tests/devices/test_firmware_download_apply.cpp` — verifies apply-and-reboot.
- `tests/integration/test_firmware_download_e2e.cpp` — full DLA flow from CoinLine Host.

## Boot-milestone dependencies

- M11 — host-call attempt to receive firmware.
- M12 — staging accesses in NVRAM/table storage region.

## Acceptance criteria

- Staged image appears in the DLA region byte-for-byte.
- Apply triggers a reset and the new ROM is loaded on next boot.
- Recovery from interrupted download leaves NVRAM in a coherent state.

## Cross-references

- [`nvram-and-table-storage.md`](nvram-and-table-storage.md).
- [`../test-plans/firmware-download-tests.md`](../test-plans/firmware-download-tests.md).
