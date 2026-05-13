# M6 Success Evidence

This document preserves the validated firmware-driven VFD state before telephony/front-panel work.

- Source run: `build/runs/20260505T105045-boot-critical`
- Firmware SHA256: `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34`
- Strict validator result: pass (`M6=1`, `0x0060 write=1`, `vfd-trace=1`)
- First M6 milestone: `boot-trace.jsonl` at `2026-05-05T13:20:50Z`
- First real firmware `0x0060` write: cycle `7473834`, data `0x14`, PC `0x5F63`, SP `0xCF9E`
- Final VFD text:
  - `Telephony board is  `
  - `   not responding   `
- Screenshot evidence: `boot-normal-start.png`, `post-install-ad-scroll.png`
- Current blocker: `telephony_board_not_responding`

Primary preserved artifact manifests:

- `build/evidence/m6-vfd-telephony-board-not-responding/m6-proof.json`
- `build/evidence/m6-vfd-current/` (latest preserved run snapshot)
