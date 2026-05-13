# Test plan: Memory / NVRAM / download family

## Unit tests

`test_nvram_persistence`, `test_nvram_corrupt_checksum`, `test_table_storage_region`, `test_firmware_download_staging`, `test_firmware_download_apply` — see `tests/devices/`.

## Failure criteria

- Staging test passes but allows unchecked overwrite of flash region in harness (must remain guarded).  
- Corrupt checksum test does not detect engineered corruption.

## MAME run evidence

Long-run folder with NVRAM file growth and checksum logs when tracing enabled.
