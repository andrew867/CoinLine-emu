# Hardware specs (`specs/hardware`)

Umbrella specification files group multiple **HW-xx-xxxxx** IDs from [`docs/status/hardware-gap-register.json`](../../docs/status/hardware-gap-register.json).

Each umbrella file contains sections **per hardware ID** with:

- Purpose  
- Hardware-facing behavior  
- Firmware-facing ports/registers / memory regions  
- Active levels, reset state, read/write behavior  
- Status bits, timing, interrupts  
- MAME source files  
- Trace events  
- Evidence artifacts  
- Tests (pointers to `tests/` and CMake targets)  
- Unknowns → [`docs/compatibility-validation-items.md`](../../docs/compatibility-validation-items.md)  
- Implementation acceptance criteria  
- Field validation acceptance criteria  

| File | IDs covered |
| ---- | ----------- |
| [HW-Z180-family.spec.md](HW-Z180-family.spec.md) | HW-CPU-001, HW-MMU-001, HW-INT-001, HW-TMR-001, HW-DMA-001, HW-RES-001 |
| [HW-MEMORY-family.spec.md](HW-MEMORY-family.spec.md) | HW-MEM-001..003, HW-NVRAM-001 |
| [HW-IO-ASCI-MODEM-family.spec.md](HW-IO-ASCI-MODEM-family.spec.md) | HW-IO-001, HW-ASCI-001, HW-MOD-001, HW-HB-001 |
| [HW-VFD-family.spec.md](HW-VFD-family.spec.md) | HW-VFD-001, HW-VFD-002 |
| [HW-INPUT-PANEL-family.spec.md](HW-INPUT-PANEL-family.spec.md) | HW-KEY-001, HW-KEY-002, HW-SEC-001, HW-MACH-001 |
| [HW-PAYMENT-family.spec.md](HW-PAYMENT-family.spec.md) | HW-CARD-001, HW-SC-001, HW-COIN-001 |
| [HW-VOICE-ALERT-family.spec.md](HW-VOICE-ALERT-family.spec.md) | HW-VOIC-001, HW-VOIC-002, HW-SPK-001, HW-DTMF-001, HW-CP-001 |
| [HW-UPD7759-VOICE-ROM-EXECUTION.spec.md](HW-UPD7759-VOICE-ROM-EXECUTION.spec.md) | **HW-VOIC-003** — uPD7759 core execution, busy/IRQ, ROM forensic integrity |
| [HW-AUDIO-TEL-SUP-family.spec.md](HW-AUDIO-TEL-SUP-family.spec.md) | HW-AUD-001, HW-TEL-001, HW-SUP-001 |
| [HW-ARTIFACTS-family.spec.md](HW-ARTIFACTS-family.spec.md) | HW-EV-001, HW-SCN-001, HW-ART-001 |
| [HW-ADSI-placeholder.spec.md](HW-ADSI-placeholder.spec.md) | HW-ADSI-001 |

Cross references: top-level `docs/*.md`, `specs/io-port-map.spec.md`, `fixtures/board/*.json`.
