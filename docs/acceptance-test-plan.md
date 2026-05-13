# Acceptance test plan

This document defines the acceptance gate for `coinline-emu`. Acceptance is reached when every item below passes. It is the union of the per-area test plans in [`../test-plans/`](../test-plans/) plus end-to-end scenarios that exercise CoinLine Host.

## Pre-conditions

- `../firmware/flash.bin` populated with a supported firmware binary; SHA-256 matches an entry in `fixtures/firmware/firmware-hashes.json`.
- `fixtures/board/board-profile-2line-vfd.json` and `fixtures/board/board-profile-11line-vfd.json` present and validated.
- `fixtures/nvram/factory-default.nvram.json` present.
- A running CoinLine Host instance reachable from the test runner.
- The emulator binary built per [`build-plan.md`](build-plan.md) and the engine track recorded in [`engine-selection.md`](engine-selection.md).

## Acceptance gates

### Gate A — Skeleton boots

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| Reset vector executes | `tests/boot/test_reset_vector.cpp` | Firmware reaches M1 deterministically. |
| Startup code begins | `tests/boot/test_first_instructions.cpp` | Firmware reaches M2 deterministically. |
| Unknown ports logged | `tests/devices/test_unknown_port_logging.cpp` | Every unknown port read/write produces a structured log line. |

### Gate B — Z180 core stable

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| MMU configured | `tests/devices/test_z180_mmu.cpp` | MMU registers match expected at M4. |
| ASCI initialized | `tests/devices/test_z180_asci.cpp` | ASCI registers match expected at M4. |
| PRT timers running | `tests/devices/test_z180_prt.cpp` | Timers fire at expected rate. |
| INT controller wired | `tests/devices/test_z180_int.cpp` | Vectors match `interrupt-map.md`. |
| DMA functional | `tests/devices/test_z180_dma.cpp` | DMA transfers complete cycle-accurately. |

### Gate C — VFD

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| First VFD write | `tests/devices/test_vfd_command_decode.cpp` | M6 reached. |
| 2-line idle snapshot | `fixtures/display/vfd-2line-idle.json` | Buffer matches expected text. |
| 11-line ad snapshot | `fixtures/display/vfd-11line-ad.json` | Buffer matches expected text. |

### Gate D — Front-panel inputs

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| Keypad scan reached | `tests/devices/test_keypad_matrix.cpp` | M7 reached. |
| Hookswitch round-trip | `tests/devices/test_hookswitch.cpp` | Lift/hangup affect firmware state. |
| Lock/door/vault/service | `tests/devices/test_security_inputs.cpp` | All four inputs read by firmware. |

### Gate E — Modem + host bridge

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| UART init | `tests/devices/test_modem_state_machine.cpp` | M8 reached. |
| Modem states | `tests/devices/test_modem_state_machine.cpp` | All declared states transition correctly. |
| Loopback round-trip | `tests/integration/test_host_bridge_loopback.cpp` | Bytes round-trip through TCP loopback. |

### Gate F — NVRAM + tables

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| NVRAM persistence | `tests/devices/test_nvram_persistence.cpp` | Image survives reset. |
| Corrupt-checksum recovery | `tests/devices/test_nvram_corrupt_checksum.cpp` | Firmware enters expected recovery path. |
| Table storage region | `tests/devices/test_table_storage_region.cpp` | Writes are bounded and persisted. |

### Gate G — Card and coin

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| Magstripe accept | `tests/devices/test_card_swipe_timing.cpp` | Valid swipe accepted. |
| Magstripe reject | `tests/devices/test_card_lrc_validation.cpp` | Invalid LRC rejected. |
| Smart card ATR | `tests/devices/test_smartcard_atr.cpp` | ATR honored. |
| Coin pulses | `tests/devices/test_coin_pulse_train.cpp` | Denominations recognized. |
| Coin errors | `tests/devices/test_coin_pulse_train.cpp` | Error paths trigger expected firmware behavior. |
| Alerter audio | `tests/devices/test_alerter_audio.cpp` | Audio events produce sample output. |

### Gate H — Boot to idle

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| Idle reached | `tests/integration/test_boot_to_idle.cpp` | M10 reached deterministically; idle VFD matches `vfd-2line-idle.json` or `vfd-11line-ad.json`. |
| Service mode entry | `tests/integration/test_service_mode_entry.cpp` | Service mode reachable from idle. |

### Gate I — CoinLine Host integration

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| Host call attempted | `tests/integration/test_modem_connect.cpp` | M11 reached against CoinLine Host. |
| Table download | `tests/integration/test_table_download_e2e.cpp` | Table reaches NVRAM/table storage; M12 reached. |
| DLOG submission | `tests/integration/test_dlog_submit_e2e.cpp` | DLOG entry observed at CoinLine Host endpoint. |

### Gate J — Scenarios + evidence

| Item | Source | Pass criterion |
| ---- | ------ | -------------- |
| Scenario suite green | `tests/integration/test_scenario_*.cpp` | All scenarios pass. |
| Evidence bundle complete | `tests/integration/test_evidence_bundle.cpp` | Bundle contains every required artifact per [`evidence-bundles.md`](evidence-bundles.md). |

## Acceptance summary

Acceptance is reached when **all** gates A through J pass on the canonical CI matrix. A reviewer signing off acceptance attaches:

- The CI run URL.
- An evidence bundle from each scenario.
- A delta against any `compatibility-validation-items.md` rows that move from `pending` to `complete`.

## Re-run cadence

- Per PR — gates A–G plus the lint job.
- Nightly — gates H, I, J, K.
- Pre-release — full gate set against a frozen firmware binary.

## Out of scope

- Field certification on physical hardware. That is a separate workflow tracked in [`compatibility-validation-items.md`](compatibility-validation-items.md). It is **never** a blocker on code merging.
- Performance benchmarking. The emulator's performance target is to keep up with the firmware's expected wall-clock behavior; a dedicated benchmark plan is out of scope for this acceptance gate.
