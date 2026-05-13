# firmware/

This directory holds the firmware images the emulator loads at startup.

## What's included

| File | Size | Origin | Notes |
| ---- | ---- | ------ | ----- |
| `flash.bin` | 512 KiB | Public hardware firmware archive (see below) | Primary flash image. |
| `flash1.bin` | 512 KiB | Public hardware firmware archive (see below) | Secondary flash image for split-image installs. |
| `voice_a.bin` | 1 MiB | Public hardware firmware archive (see below) | Primary voiceware ROM image. |
| `voice_b.bin` | 1 MiB | Public hardware firmware archive (see below) | Secondary voiceware ROM image. |
| `telephony_subprocessor.rom` | 4 KiB | Built from original source in [`tools/tp8048/`](../tools/tp8048/) | Behavioural model of the on-board telephony co-processor. Distributed under the repository's GPL-2.0-or-later license. |

## Where the flash/voiceware blobs come from

The flash and voiceware images here are **publicly archived hardware firmware** — the same blobs that have been mirrored online for years by the preservation community. They are available from the CCC Munich wiki at <https://wiki.muc.ccc.de/millennium:firmwareversions> and from several Millennium-related GitHub mirrors.

They are included here only to let a fresh checkout boot and exercise the emulator end-to-end during development. They will be **removed for the v1 tagged release**, and contributors are expected to bring their own images for further work.

## Override paths

Each image can be overridden at runtime via environment variables — see [`../RUNNING.md`](../RUNNING.md) for the full list. Common overrides:

- `COINLINE_FIRMWARE` / `COINLINE_FIRMWARE_FLASH0` — primary flash image
- `COINLINE_FIRMWARE_FLASH1` — secondary flash image (split-image installs)
- `COINLINE_VOICE_ROM_A` / `COINLINE_VOICE_ROM_B` — voiceware ROM images
