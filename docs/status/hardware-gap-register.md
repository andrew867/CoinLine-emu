# Hardware gap register (summary)

**Source of truth:** [`hardware-gap-register.json`](hardware-gap-register.json) (machine-readable).  
**Audit date:** 2026-05-04  
**Reference run:** `build/runs/20260504T135115-post-mmu-boot-gate`  
**Highest milestone (measured):** M5 — **M6 not reached** (no `vfd_data` / first real `0x60` VFD data write in boot trace for that window).

## Category counts (from JSON)

| Category | Count |
| -------- | ----- |
| complete_verified | 8 |
| complete_unverified | 3 |
| partial | 22 |
| stub | 0 |
| docs_only | 0 |
| fixture_only | 0 |
| blocked | 0 |
| unknown | 2 |
| **Total** | **35** |

## Rules used

- **complete_verified:** MAME code present, linked in `cl_millennium`, builds, and **unit/integration tests in-repo** back the behavior; where the item depends on a full firmware run, additional **measured run artifacts** exist in `docs/status/` or `build/runs/`.
- **complete_unverified:** Implementation exists; **proof bundle incomplete** (no mandatory Class C run or no benchmark trace).
- **partial:** Gaps in behavior, tests, traces, or **boot path** (e.g. **M6/M10 not observed**).
- **unknown / blocked:** See per-row notes in JSON.

## Excerpt table (see JSON for full columns)

| ID | Item | Status | Tranche | Primary gap |
| --- | --- | --- | --- | --- |
| HW-CPU-001 | Z180 core | complete_verified | H1 | upstream MAME |
| HW-MMU-001 | MMU | complete_verified | H1 | edge-case bank tests |
| HW-MEM-001 | Flash/ROM map | complete_verified | H1 | alt images |
| HW-MEM-002 | RAM / stack | complete_verified | H1 | — |
| HW-MEM-003 | Download staging | partial | H11 | safe flash policy |
| HW-INT-001 | Interrupt ITC/IL | partial | H2 | EI/IRQ path vs traces |
| HW-TMR-001 | Timers / PRT | partial | H2 | OS tick correlation |
| HW-DMA-001 | DMA | complete_unverified | H1 | firmware may not use |
| HW-ASCI-001 | ASCI / UART | partial | H3 | modem readiness vs boot |
| HW-MOD-001 | Modem line model | partial | H3 | field_validation_pending |
| HW-HB-001 | Host bridge TCP | partial | H12 | CI Host availability |
| HW-VFD-001 | 2-line VFD | partial | H4 | **M6 not reached** |
| HW-VFD-002 | 11-line VFD | partial | H4 | profile not proven |
| HW-KEY-001 | Keypad matrix | complete_verified | H5 | bounce profiling |
| HW-KEY-002 | Quick / vol / lang / hook | complete_verified | H5 | E2E call scenarios |
| HW-SEC-001 | Lock/door/vault/service | complete_verified | H5 | field_validation_pending |
| HW-MACH-001 | MACH PIO 0xC0 | partial | H5 | firmware branch coverage |
| HW-CARD-001 | Magstripe | partial | H9 | head waveform fidelity |
| HW-SC-001 | Smartcard | partial | H9 | ISO7816 electrical |
| HW-COIN-001 | Coin validator | partial | H10 | denom proof |
| HW-NVRAM-001 | NVRAM file | partial | H11 | wear/format parity |
| HW-SPK-001 | Alerter | partial | H6 | SPL match |
| HW-VOIC-001 | Voiceware device | partial | H6 | phrase catalog parity |
| HW-VOIC-002 | U16/U26 ROM | complete_unverified | H6 | bench checksum optional |
| HW-AUD-001 | Audio routing | partial | H7 | sidetone levels |
| HW-TEL-001 | Telephony decode | partial | H7 | live PSTN |
| HW-SUP-001 | Disconnect supervision | partial | H8 | wire-level CPC |
| HW-RES-001 | Reset/watchdog | unknown | H1 | IC-level timeout |
| HW-IO-001 | Unknown port log | complete_verified | H0 | — |
| HW-ART-001 | Artwork/layout | partial | H14 | clickable bezel |
| HW-EV-001 | Evidence bundles | partial | H13 | optional traces |
| HW-SCN-001 | Screenshot harness | complete_unverified | H14 | CI display |
| HW-CP-001 | Call progress tones | partial | H7 | ITU accuracy |
| HW-ADSI-001 | ADSI | unknown | — | no firmware proof |
| HW-DTMF-001 | DTMF path | partial | H6 | line vs speaker routing evidence |

## Commit IDs

Populate `commit_after_completion` in the JSON as tranches land; this markdown table does not duplicate git hashes (avoid staleness).
