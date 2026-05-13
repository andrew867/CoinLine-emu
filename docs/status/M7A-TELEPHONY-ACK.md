# M7A Telephony ACK (Evidence)

Evidence source run: `build/runs/20260505T140947-boot-critical` (30s, `TraceProfile=uart`).

## What Was Proven

- M6_VFD_BOOT: PASS (strict validator passes; firmware-driven port `0x0060` writes present).
- M7A_TELEPHONY_ACK: PASS.
  - Firmware consumed `0x72` (POWER_ON_ACK) from port `0x00E1` (captured in `external-uart-trace.jsonl`).

## What Is Not Yet Proven (M7B)

- M7B_TELEPHONY_READY: NOT REACHED.
  - Firmware still ends on the VFD screen:
    - `Telephony board is`
    - `not responding`

## Evidence Bundle

Folder: `build/evidence/m7a-telephony-ack/`

- `m7a-proof.json`
- `boot-milestones.json`
- `boot-trace.jsonl`
- `external-uart-trace.jsonl`
- `vfd-trace.jsonl`
- `vfd-final-text.txt`
- `final-screen.png`
- `build-result.json`

