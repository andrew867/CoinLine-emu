# NVRAM and table storage

## Cold boot

Start with erased serial EEPROM and blank battery-backed SRAM when a profile has no battery file. The serial EEPROM begins with `0xffff` words. The MTR 1.9 demo wrote EEPROM through Microwire and copied its compiled defaults to SRAM. Its first full INSTALL still downloaded the lab records from CoinLine. The OEM also performed real INSTALL writes.

## Persistence

MAME saves the serial EEPROM and firmware-written SRAM together in the `battery` image. The portable launcher keeps these files separate for the two firmware profiles.

| Profile | Battery path relative to the portable folder |
| --- | --- |
| Demo | `data/demo/nvram/cl_millennium/battery` |
| OEM | `data/oem/nvram/cl_millennium/battery` |

A successful warm boot uses the already-written memory and can enter service directly. Close the emulator and back up or transfer the entire profile directory to preserve that state. A fresh extraction has its own blank profiles.

The firmware owns all table CRCs, loaded flags, and the installation state. The packed host table records reach SRAM using the ordinary NCC transfer and firmware handlers. The lab verified table readback and a warm return to **Please lift receiver**.

## Explicit test fixtures

The NVRAM model tests use fixture images for startup, invalid checksums, and recovery cases. The normal portable launch starts with its native battery image or blank storage. It supplies neither a fixture battery image nor preset installation flags.

See [table download behavior](table-download-behavior.md) and the [verified cold and warm runs](status/ncc-install-firmware-stx-handoff-2026-11-23.md).
