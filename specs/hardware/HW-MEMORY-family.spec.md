# Umbrella spec: ROM/RAM, banking, NVRAM, download staging

Hardware IDs: **HW-MEM-001**, **HW-MEM-002**, **HW-MEM-003**, **HW-NVRAM-001**

## HW-MEM-001 — Flash / ROM mapping

- **Regions:** `millennium_memory.cpp`, ROM in `millennium.cpp`.  
- **Evidence:** `test_memory_map_boot_regions.cpp`; firmware entry vectors readable.  
- **Acceptance:** Address map matches internal memory-map doc for standard image.

## HW-MEM-002 — RAM and stack persistence

- **Evidence:** `memory-trace.jsonl`, `test_stack_ram_mapping.cpp`, post-MMU run notes.  
- **Acceptance:** Writes in low logical RAM visible across instruction streams (no phantom reset clearing stack).

## HW-MEM-003 / HW-NVRAM-001 — Table storage, download staging, NVRAM image

- **Sources:** `millennium_nvram*.cpp`, `millennium_firmware.cpp`.  
- **Behavior:** File-backed NVRAM; table checksums per `docs/nvram-and-table-storage.md`.  
- **Tests:** `test_nvram_persistence.cpp`, `test_table_storage_region.cpp`, `test_firmware_download_staging.cpp`, `test_nvram_corrupt_checksum.cpp`, `test_firmware_download_apply.cpp`.  
- **Unknowns:** Real flash programming voltages/timings — **not** emulated at electrical level.  
- **Acceptance:** Staging does not corrupt primary flash image in default harness; corrupt tests prove detection path.  
- **Field validation:** Compare NVRAM file to hardware dump after same operations.
