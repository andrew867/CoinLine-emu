# M7 Telephony Success (Evidence)

Evidence source run: `build/runs/20260505T140947-boot-critical` (30s, `TraceProfile=uart`).

## What Was Proven

- M6_VFD_BOOT: PASS (strict validator passes; firmware-driven port `0x0060` writes present).
- M7_TELEPHONY_BOARD_RESPONDS: PASS.
  - Firmware read byte `0x72` (POWER_ON_ACK) from port `0x00E1` (external UART path).
  - Protocol frames modeled (trace-backed IDs):
    - `0x72` = POWER_ON_ACK
    - `0xC0` = TELEPHONY_STATUS
    - `0xC2` = TELEPHONY_VERSION_NUMBER

## Evidence Bundle

Folder: `build/evidence/m7-telephony-board-responds/`

- `m7-proof.json` (summary + decode + first response read)
- `boot-milestones.json`
- `boot-trace.jsonl`
- `external-uart-trace.jsonl`
- `vfd-trace.jsonl`
- `vfd-final-text.txt`
- `final-screen.png`

## Current Gate After M7

Even with M7 recorded (POWER_ON_ACK read), the firmware still ends on the telephony-not-responding screen in this run. Next targets are M8 (front-panel inputs) and M9 (voiceware audio/WAV capture), but telephony protocol progression beyond the initial ACK likely still needs work to reach a stable interactive prompt.

