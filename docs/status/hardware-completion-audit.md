# Hardware completion audit (Phase 1)

**Project:** CoinLine Terminal Emulator — `coinline-emu`  
**Method:** Read `docs/`, `specs/`, `test-plans/`, `fixtures/`, `src/mame/coinline/`, `tests/`, and measured status in `docs/status/implementation-status-audit.md` and `docs/status/boot-milestone-status.json`.  
**Firmware reference:** `firmware/` (`flash.bin` SHA-256 in gap register JSON).

This audit **does not** claim hardware-complete status for the terminal. The highest **measured** boot milestone in the reference capture remains **M5**; **M6** (first real VFD data / `0x60` path proof) and **M10** are **not** asserted.

## Completion table

Columns match the mission template. **Commit ID** is maintained in [`hardware-gap-register.json`](hardware-gap-register.json) per row (`commit_after_completion`); global audit commit is recorded in [`hardware-final-status.md`](hardware-final-status.md).

| ID | Hardware item | Files present | Docs/specs | Tests | Fixtures | Current status | Missing implementation | Missing tests | Missing traces/evidence | Firmware/source evidence | MAME target | Risk | Priority | Tranche | Commit after |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| HW-CPU-001 | Z180 core | Y | Y | Y | board profiles | complete_verified | — | chip-level N/A | optional | firmware source tree build | z80180_device | low | P3 | H1 | TBD |
| HW-MMU-001 | MMU | Y | Y | Y | Y | complete_verified | rare CB combos | permutations | optional extra | traces | millennium MMU | medium | P1 | H1 | TBD |
| HW-MEM-001 | Flash ROM map | Y | Y | Y | — | complete_verified | NLA alt path | optional | memory-trace | flash.bin | address_map | low | P2 | H1 | TBD |
| HW-MEM-002 | RAM / stack | Y | Y | Y | — | complete_verified | size parity | soak | extended trace | stack mapping doc | RAM regions | medium | P1 | H1 | TBD |
| HW-MEM-003 | Download staging | Y | Y | Y | nvram fixtures | partial | safe flash | Class C | nvram-trace | firmware source tree dl | nvram+firmware | high | P1 | H11 | TBD |
| HW-INT-001 | Interrupt controller | Y | Y | Y | — | partial | EI/IRQ vs boot | RETI trace | interrupt-events | RTOS maps | Z180 internal | high | P0 | H2 | TBD |
| HW-TMR-001 | PRT / timers | Y | Y | Y | — | partial | tick vs OS | integration | timer + ctx | correlation docs | Z180 PRT | medium | P1 | H2 | TBD |
| HW-DMA-001 | DMA | upstream | Y | Y | — | complete_unverified | firmware use? | capture | — | TBD scan | Z180 DMA | low | P4 | H1 | TBD |
| HW-ASCI-001 | ASCI UART | Y | Y | Y | modem JSON | partial | boot gate | long NCC | asci + uart | firmware source tree | modem+bridge | high | P0 | H3 | TBD |
| HW-MOD-001 | Modem model | Y | Y | Y | — | partial | PSTN realism | bench | modem in io-trace | fixtures | modem_model | medium | P1 | H3 | TBD |
| HW-HB-001 | Host bridge | Y | Y | Y | — | partial | protocol versions | CI Host | transcript | Host docs | TCP bridge | medium | P1 | H12 | TBD |
| HW-VFD-001 | 2-line VFD | Y | Y | Y | display JSON | partial | **M6** | Class C | vfd_data | stops at M5 | vfd + screen | high | P0 | H4 | TBD |
| HW-VFD-002 | 11-line VFD | Y | Y | Y | 11-line profile | partial | boot path | scenario | vfd-trace | fixture | vfd_model | medium | P2 | H4 | TBD |
| HW-KEY-001 | Keypad matrix | Y | Y | Y | — | complete_verified | bounce | LA capture | io-trace | firmware source tree | keypad | low | P2 | H5 | TBD |
| HW-KEY-002 | Quick/vol/lang/hook | Y | Y | Y | — | complete_verified | E2E call | M6+ | audio trace | KEYMATRIX | ioports | low | P2 | H5 | TBD |
| HW-SEC-001 | Security inputs | Y | Y | Y | — | complete_verified | physical bounce | field | io | firmware source tree | security_model | low | P3 | H5 | TBD |
| HW-MACH-001 | MACH PIO | Y | Y | Y | — | partial | branch coverage | regress | mach_pio tag | fixtures / traces | mach_pio | medium | P1 | H5 | TBD |
| HW-CARD-001 | Magstripe | Y | Y | Y | card | partial | head fidelity | integration | card-trace | firmware source tree | card | medium | P2 | H9 | TBD |
| HW-SC-001 | Smartcard | Y | Y | Y | — | partial | ISO layer | ATR bench | ext traces | firmware source tree | smartcard | high | P2 | H9 | TBD |
| HW-COIN-001 | Coin validator | Y | Y | Y | — | partial | denom proof | paid call | coin-trace | firmware source tree | coin | medium | P2 | H10 | TBD |
| HW-NVRAM-001 | NVRAM | Y | Y | Y | — | partial | flash IC parity | soak | optional | tables | nvram | medium | P2 | H11 | TBD |
| HW-SPK-001 | Alerter | Y | Y | Y | alerter JSON | partial | SPL | WAV bench | alerter-trace | ports | audio_dev | low | P2 | H6 | TBD |
| HW-VOIC-001 | Voiceware | Y | Y | Y | VW maps | partial | full catalog | Class C WAV | voiceware trace | ROMs + B3 doc | voiceware dev | medium | P1 | H6 | TBD |
| HW-VOIC-002 | Voice ROMs | Y | Y | Y | — | complete_unverified | ROM probe | optional | bank trace | CRC in driver | ROM regions | low | P2 | H6 | TBD |
| HW-AUD-001 | Audio routing | Y | Y | Y | route JSON | partial | analog levels | skip-77 tests | route traces | JSON map | aud_route | medium | P1 | H7 | TBD |
| HW-TEL-001 | Telephony decode | Y | Y | Y | — | partial | PSTN scope | live | telephony trace | partial sources | telephony | medium | P2 | H7 | TBD |
| HW-SUP-001 | Supervision | Y | Y | Y | disconnect JSON | partial | CPC detail | teardown | supervision trace | map JSON | supervision | medium | P2 | H8 | TBD |
| HW-RES-001 | Reset/watchdog | partial | Y | partial | — | unknown | timeout IC | capture | reset-trace | incomplete | state | medium | P3 | H1 | TBD |
| HW-IO-001 | Unknown port log | Y | Y | Y | io map | complete_verified | — | fuzz optional | unknown json | boots | catch_all | low | P3 | H0 | TBD |
| HW-ART-001 | Artwork/layout | Y | Y | Y | — | partial | bezel hit tests | visual | — | — | layout | low | P4 | H14 | TBD |
| HW-EV-001 | Evidence bundles | Y | Y | Y | scenarios | partial | all trace kinds | matrix | optional files | runs | exporter | low | P2 | H13 | TBD |
| HW-SCN-001 | Screenshot script | Y | Y | env | — | complete_unverified | CI display | pixel diff | PNG | run folder | script | low | P3 | H14 | TBD |
| HW-CP-001 | Call progress model | Y | Y | Y | — | partial | ITU accuracy | FFT | audio trace | model | audio_model | low | P4 | H7 | TBD |
| HW-ADSI-001 | ADSI | — | — | — | — | unknown | scope | — | — | none | — | low | P5 | blocked | TBD |
| HW-DTMF-001 | DTMF | Y | Y | Y | — | partial | line routing | HW | alerter trace | ports | audio | low | P4 | H6 | TBD |

## Legend

- **Y** = present at audit time (see JSON for paths).
- **TBD** = fill `commit_after_completion` when a tranche closes that item’s gaps.

## Definition of 100% (strict)

A hardware line item reaches **complete_verified** only when **all** apply:

1. Source exists under `src/mame/coinline/` or upstream MAME as cited.  
2. Wired into `cl_millennium` / machine config.  
3. **Build succeeds** (`build-mame-coinline.ps1` path).  
4. **Automated tests** exist where meaningful (unit/device/integration per TESTING.md).  
5. **Trace or run artifact** supports firmware-visible behavior for that item when the item is firmware-driven.  
6. **Specs/docs** updated when behavior is asserted.  
7. **No fabricated** VFD/voice/modem/modem-traffic behavior; unknowns appear under `docs/compatibility-validation-items.md` or item `notes`.  
8. If only bench/field tests remain, status is **complete_verified** for emulation + **field_validation_pending** flag in JSON (not “implementation incomplete”).

See [`hardware-validation-matrix.md`](hardware-validation-matrix.md) for trace-file expectations vs IDs.

