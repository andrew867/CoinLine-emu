# Hardware Final Status

**Last updated:** 2026-05-05

## Latest Proven Boot-Critical Snapshot

| Field | Value |
| ----- | ----- |
| Latest run | `build/runs/20260505T140947-boot-critical` |
| Latest build | Pass, SHA256 `a1254ca5e8ba1fbf3faf2c104eccfc648509d808ffd98127645f6d62881287a1` |
| M6_VFD_BOOT | PASS (firmware-driven port `0x0060` writes present; strict validator passes) |
| M7_TELEPHONY_BOARD_RESPONDS | PASS (firmware read `0x72` from external UART on port `0x00E1`) |
| M10_IDLE_DEMO_PROMPT | Not reached |

## Hardware Bring-up Notes (Evidence-Backed)

- Z180 internal MMU/ITC register readback is coherent and trace tags are decoded (previous regression fixed).
- PC `0xFFFF` / RST38 fault loop is resolved (no longer the active boot gate).
- Current user-visible VFD screen still indicates telephony is not up (see `vfd-final-text.txt` in the latest run).

## Current Blocking Hardware Gate Toward Interactive Use

Telephony protocol progression beyond POWER_ON_ACK (`0x72`) is incomplete from the firmware's perspective: the VFD remains on the "not responding" screen even though M7 is recorded.
