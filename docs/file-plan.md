# Master file plan

This document is the single source of truth for every file (planned or created) in `coinline-emu/`. Tranche numbers map to [`tranche-plan.md`](tranche-plan.md).

## Legend

- **Create/Modify**: `C` for files this project creates, `M` for files this project modifies, `P` for planned (not yet materialized), `D` for documentation already created in the docs-only pass.
- **Tranche**: implementation tranche where the file is brought to life (or `docs` if delivered in the docs-only pass).
- **Tests**: file path of the primary test that exercises the file (planned).

## Top-level

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `coinline-emu/LICENSE-STRATEGY.md` | D | License posture and rationale | docs | n/a | Public; the single source of truth on licensing. |
| `coinline-emu/BUILDING.md` | D | Build prerequisites and command sequences | docs | n/a | Updated when build lands in E0/E2. |
| `coinline-emu/RUNNING.md` | D | How to launch the emulator | docs | n/a | Updated when CLI lands in E2. |
| `coinline-emu/TESTING.md` | D | How to run tests | docs | n/a | Updated as test corpus grows. |
| `coinline-emu/LICENSE` | P | License file (GPL-2.0-or-later, MAME track) | E0 | n/a | |

## docs/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `docs/README.md` | D | docs/ index | docs | n/a | |
| `docs/architecture.md` | D | Runtime architecture and process boundary | docs | n/a | |
| `docs/engine-selection.md` | D | Engine comparison + recommendation | docs | n/a | |
| `docs/project-structure.md` | D | Source-tree, fixtures, artwork layout | docs | n/a | |
| `docs/implementation-roadmap.md` | D | High-level path E0–E13 | docs | n/a | |
| `docs/tranche-plan.md` | D | Per-tranche goals and exit criteria | docs | n/a | |
| `docs/file-plan.md` | D | This file | docs | n/a | Updated whenever a file is added or removed. |
| `docs/build-plan.md` | D | Build-system rollout plan | docs | n/a | |
| `docs/test-strategy.md` | D | Global test approach | docs | n/a | |
| `docs/acceptance-test-plan.md` | D | Acceptance gate definition | docs | n/a | |
| `docs/boot-milestones.md` | D | M0–M12 ladder | docs | `tests/boot/*` | |
| `docs/debugging-guide.md` | D | Debug workflows | docs | n/a | |
| `docs/risk-register.md` | D | Risks + mitigations | docs | n/a | |
| `docs/compatibility-validation-items.md` | D | Compatibility item ledger | docs | per-row test references | |
| `docs/host-integration-plan.md` | D | Path to CoinLine Host integration | docs | `tests/integration/host_*` | |
| `docs/evidence-bundles.md` | D | Evidence bundle definition | docs | `specs/evidence-bundle.spec.md` | |
| `docs/scenario-runner.md` | D | Scenario verb set and JSON schema | docs | `specs/scenario-runner.spec.md` | |
| `docs/mame-driver-plan.md` | D | MAME driver layout and registration | docs | n/a | |
| `docs/board-profiles.md` | D | Board profile definitions | docs | `tests/devices/test_board_profile_*.cpp` | |
| `docs/memory-map.md` | D | Address map | docs | `tests/devices/test_memory_*.cpp` | |
| `docs/io-port-map.md` | D | I/O port map | docs | `tests/devices/test_io_port_*.cpp` | |
| `docs/interrupt-map.md` | D | Interrupt vectors | docs | `tests/devices/test_interrupts_*.cpp` | |
| `docs/z180-core.md` | D | Z180 CPU core expectations | docs | `tests/devices/test_z180_*.cpp` | |
| `docs/z180-internal-peripherals.md` | D | MMU/ASCI/PRT/INT/DMA | docs | `tests/devices/test_z180_*.cpp` | |
| `docs/device-model.md` | D | Common device contract | docs | n/a | |
| `docs/vfd-emulation.md` | D | 2-line + 11-line VFD | docs | `tests/devices/test_vfd_*.cpp` | |
| `docs/keypad-emulation.md` | D | Keypad matrix | docs | `tests/devices/test_keypad_*.cpp` | |
| `docs/hookswitch-and-handset.md` | D | Hookswitch + handset audio | docs | `tests/devices/test_hookswitch.cpp` | |
| `docs/card-reader-emulation.md` | D | Magstripe reader | docs | `tests/devices/test_card_*.cpp` | |
| `docs/smartcard-emulation.md` | D | Smart card / memory card | docs | `tests/devices/test_smartcard_*.cpp` | |
| `docs/coin-validator-emulation.md` | D | Coin validator | docs | `tests/devices/test_coin_*.cpp` | |
| `docs/alerter-audio.md` | D | Alerter audio | docs | `tests/devices/test_alerter_*.cpp` | |
| `docs/lock-door-vault-service.md` | D | Security inputs | docs | `tests/devices/test_security_*.cpp` | |
| `docs/modem-uart-host-bridge.md` | D | Modem + host bridge | docs | `tests/devices/test_modem_*.cpp` | |
| `docs/nvram-and-table-storage.md` | D | NVRAM + tables | docs | `tests/devices/test_nvram_*.cpp` | |
| `docs/firmware-download-storage.md` | D | DLA staging | docs | `tests/devices/test_firmware_download_*.cpp` | |
| `docs/table-download-behavior.md` | D | Table download flow | docs | `tests/integration/test_table_download_*.cpp` | |
| `docs/artwork-and-layout.md` | D | MAME `.lay` plan | docs | `tests/integration/test_artwork_*.cpp` | |
| `docs/frontend-control-panel.md` | D | Optional control-panel UI | docs | n/a | |
| `docs/ci-and-release.md` | D | CI / release plan | docs | n/a | |
| `docs/audio-mame-device-architecture.md` | D | Audio devices in MAME driver | docs | n/a | |
| `docs/audio-boot-integration-plan.md` | D | Audio milestones M6A–M11D vs boot ladder | docs | n/a | |
| `docs/audio-debugging-and-trace-plan.md` | D | Audio trace debugging workflow | docs | n/a | |
| `docs/audio-evidence-bundle-plan.md` | D | Audio evidence artifacts | docs | n/a | |
| `docs/audio-ci-plan.md` | D | Audio CI regression tiers | docs | n/a | |
| `docs/voiceware-emulation.md` | D | Voice playback emulation | docs | `test-plans/voiceware-tests.md` | |
| `docs/audio-routing-emulation.md` | D | Audio routing emulation | docs | `test-plans/audio-routing-tests.md` | |
| `docs/disconnect-supervision-emulation.md` | D | Disconnect supervision emulation | docs | `test-plans/disconnect-supervision-tests.md``specs/mame-machine-driver.spec.md` | D | MAME driver contract | docs | n/a | |
| `specs/firmware-loader.spec.md` | D | Firmware loader contract | docs | `tests/boot/test_firmware_load.cpp` | |
| `specs/z180-board-profile.spec.md` | D | Board profile schema | docs | `tests/fixtures/test_board_profile_schema.py` | |
| `specs/memory-map.spec.md` | D | Memory map JSON schema | docs | `tests/fixtures/test_memory_map_schema.py` | |
| `specs/io-port-map.spec.md` | D | I/O port map JSON schema + unknown-port policy | docs | `tests/fixtures/test_io_port_map_schema.py` | |
| `specs/vfd-device.spec.md` | D | VFD contract | docs | `tests/devices/test_vfd_*.cpp` | |
| `specs/keypad-device.spec.md` | D | Keypad contract | docs | `tests/devices/test_keypad_*.cpp` | |
| `specs/card-reader-device.spec.md` | D | Magstripe contract | docs | `tests/devices/test_card_*.cpp` | |
| `specs/smartcard-device.spec.md` | D | Smart-card contract | docs | `tests/devices/test_smartcard_*.cpp` | |
| `specs/coin-validator-device.spec.md` | D | Coin validator contract | docs | `tests/devices/test_coin_*.cpp` | |
| `specs/modem-uart-device.spec.md` | D | Modem UART contract | docs | `tests/devices/test_modem_*.cpp` | |
| `specs/nvram-storage-device.spec.md` | D | NVRAM contract | docs | `tests/devices/test_nvram_*.cpp` | |
| `specs/alerter-audio-device.spec.md` | D | Alerter contract | docs | `tests/devices/test_alerter_*.cpp` | |
| `specs/lock-door-service-device.spec.md` | D | Security inputs contract | docs | `tests/devices/test_security_*.cpp` | |
| `specs/host-bridge.spec.md` | D | Host bridge wire protocol | docs | `tests/integration/test_host_bridge_*.cpp` | |
| `specs/scenario-runner.spec.md` | D | Scenario JSON schema | docs | `tests/fixtures/test_scenario_schema.py` | |
| `specs/evidence-bundle.spec.md` | D | Evidence bundle schema | docs | `tests/fixtures/test_evidence_bundle_schema.py` | |
| `specs/debugging-tools.spec.md` | D | Debug tool contracts | docs | tool-specific tests | |
| `specs/voiceware-device.spec.md` | D | Voiceware logical contract | docs | `tests/devices/test_voiceware_*.cpp` | Pre-MAME behavioral spec. |
| `specs/audio-routing-device.spec.md` | D | Audio routing logical contract | docs | `tests/devices/test_audio_route_*.cpp` | |
| `specs/disconnect-supervision-device.spec.md` | D | Supervision logical contract | docs | `tests/devices/test_supervision_*.cpp` | |
| `specs/voiceware-mame-device-implementation.spec.md` | D | Voiceware MAME device implementation | A1 | `tests/devices/test_voiceware_*.cpp` | |
| `specs/audio-routing-mame-device-implementation.spec.md` | D | Audio route MAME implementation | A2 | `tests/devices/test_audio_route_*.cpp` | |
| `specs/disconnect-supervision-mame-device-implementation.spec.md` | D | Supervision MAME implementation | A4 | `tests/devices/test_supervision_*.cpp` | |
| `specs/alerter-audio-mame-device-implementation.spec.md` | D | Alerter MAME extension | A3 | `tests/devices/test_alerter_*.cpp` | |
| `specs/audio-call-state-integration.spec.md` | D | Cross-device call-state expectations | A5 | `tests/integration/test_audio_call_state_*.cpp` | |
| `specs/audio-trace-format.spec.md` | D | JSONL trace schema | A1+ | parser tests | |

## test-plans/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `test-plans/boot-tests.md` | D | Boot-tier plan | docs | `tests/boot/*` | |
| `test-plans/cpu-and-z180-register-tests.md` | D | CPU/Z180 register plan | docs | `tests/devices/test_z180_*.cpp` | |
| `test-plans/memory-map-tests.md` | D | Memory map plan | docs | `tests/devices/test_memory_*.cpp` | |
| `test-plans/io-port-tests.md` | D | I/O port plan | docs | `tests/devices/test_io_port_*.cpp` | |
| `test-plans/vfd-tests.md` | D | VFD plan | docs | `tests/devices/test_vfd_*.cpp` | |
| `test-plans/keypad-tests.md` | D | Keypad plan | docs | `tests/devices/test_keypad_*.cpp` | |
| `test-plans/hookswitch-tests.md` | D | Hookswitch plan | docs | `tests/devices/test_hookswitch.cpp` | |
| `test-plans/card-reader-tests.md` | D | Card reader plan | docs | `tests/devices/test_card_*.cpp` | |
| `test-plans/smartcard-tests.md` | D | Smart-card plan | docs | `tests/devices/test_smartcard_*.cpp` | |
| `test-plans/coin-validator-tests.md` | D | Coin plan | docs | `tests/devices/test_coin_*.cpp` | |
| `test-plans/modem-uart-tests.md` | D | Modem UART plan | docs | `tests/devices/test_modem_*.cpp` | |
| `test-plans/nvram-storage-tests.md` | D | NVRAM plan | docs | `tests/devices/test_nvram_*.cpp` | |
| `test-plans/firmware-download-tests.md` | D | DLA plan | docs | `tests/devices/test_firmware_download_*.cpp` | |
| `test-plans/table-download-tests.md` | D | Table download plan | docs | `tests/integration/test_table_download_*.cpp` | |
| `test-plans/host-integration-tests.md` | D | Host integration plan | docs | `tests/integration/test_host_*.cpp` | |
| `test-plans/scenario-runner-tests.md` | D | Scenario runner plan | docs | `tests/integration/test_scenario_*.cpp` | |
| `test-plans/artwork-layout-tests.md` | D | Artwork plan | docs | `tests/integration/test_artwork_*.cpp` | |
| `test-plans/regression-tests.md` | D | Regression plan | docs | `tests/integration/test_regression_*.cpp` | |
| `test-plans/acceptance-tests.md` | D | Acceptance plan | docs | `tests/integration/test_acceptance_*.cpp` | |
| `test-plans/voiceware-mame-implementation-tests.md` | D | Voiceware MAME tests | A1 | `tests/devices/test_voiceware_*.cpp` | Class A vs C separation. |
| `test-plans/audio-routing-mame-implementation-tests.md` | D | Audio route MAME tests | A2 | `tests/devices/test_audio_route_*.cpp` | |
| `test-plans/disconnect-supervision-mame-implementation-tests.md` | D | Supervision MAME tests | A4 | `tests/devices/test_supervision_*.cpp` | |
| `test-plans/alerter-audio-mame-implementation-tests.md` | D | Alerter MAME tests | A3 | `tests/devices/test_alerter_*.cpp` | |
| `test-plans/audio-call-state-integration-tests.md` | D | Call-state integration tests | A5 | `tests/integration/test_audio_call_state_*.cpp` | |
| `test-plans/audio-boot-regression-tests.md` | D | Boot + audio regression | A7 | milestone validator | |

## src/mame/coinline/ (planned)

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `src/mame/coinline/millennium.cpp` | P | Driver entry | E2 | `tests/boot/test_reset_vector.cpp` | |
| `src/mame/coinline/millennium.h` | P | Driver header | E2 | n/a | |
| `src/mame/coinline/millennium_state.cpp` | P | Machine state container | E2 | `tests/boot/test_first_instructions.cpp` | |
| `src/mame/coinline/millennium_state.h` | P | State header | E2 | n/a | |
| `src/mame/coinline/millennium_memory.cpp` | P | Address map | E2 | `tests/devices/test_memory_map.cpp` | |
| `src/mame/coinline/millennium_memory.h` | P | Memory header | E2 | n/a | |
| `src/mame/coinline/millennium_io.cpp` | P | I/O map + unknown-port logger | E2 | `tests/devices/test_unknown_port_logging.cpp` | |
| `src/mame/coinline/millennium_io.h` | P | I/O header | E2 | n/a | |
| `src/mame/coinline/millennium_vfd.cpp` | P | VFD device | E4 | `tests/devices/test_vfd_*.cpp` | |
| `src/mame/coinline/millennium_vfd.h` | P | VFD header | E4 | n/a | |
| `src/mame/coinline/millennium_keypad.cpp` | P | Keypad + hookswitch | E5 | `tests/devices/test_keypad_*.cpp` | |
| `src/mame/coinline/millennium_keypad.h` | P | Keypad header | E5 | n/a | |
| `src/mame/coinline/millennium_card.cpp` | P | Magstripe reader | E8 | `tests/devices/test_card_*.cpp` | |
| `src/mame/coinline/millennium_card.h` | P | Card header | E8 | n/a | |
| `src/mame/coinline/millennium_smartcard.cpp` | P | Smart card | E8 | `tests/devices/test_smartcard_*.cpp` | |
| `src/mame/coinline/millennium_smartcard.h` | P | Smart card header | E8 | n/a | |
| `src/mame/coinline/millennium_coin.cpp` | P | Coin validator | E9 | `tests/devices/test_coin_*.cpp` | |
| `src/mame/coinline/millennium_coin.h` | P | Coin header | E9 | n/a | |
| `src/mame/coinline/millennium_modem.cpp` | P | Modem glue | E6 | `tests/devices/test_modem_*.cpp` | |
| `src/mame/coinline/millennium_modem.h` | P | Modem header | E6 | n/a | |
| `src/mame/coinline/millennium_nvram.cpp` | P | NVRAM + table storage | E7 | `tests/devices/test_nvram_*.cpp` | |
| `src/mame/coinline/millennium_nvram.h` | P | NVRAM header | E7 | n/a | |
| `src/mame/coinline/millennium_audio.cpp` | P | Alerter + handset audio | E9/A3 | `tests/devices/test_alerter_*.cpp` | Extended in A3. |
| `src/mame/coinline/millennium_audio.h` | P | Audio header | E9/A3 | n/a | |
| `src/mame/coinline/millennium_voiceware.cpp` | P | Voiceware device | A1 | `tests/devices/test_voiceware_*.cpp` | |
| `src/mame/coinline/millennium_voiceware.h` | P | Voiceware header | A1 | n/a | |
| `src/mame/coinline/millennium_audio_route.cpp` | P | Audio routing device | A2 | `tests/devices/test_audio_route_*.cpp` | |
| `src/mame/coinline/millennium_audio_route.h` | P | Audio routing header | A2 | n/a | |
| `src/mame/coinline/millennium_supervision.cpp` | P | Disconnect supervision device | A4 | `tests/devices/test_supervision_*.cpp` | |
| `src/mame/coinline/millennium_supervision.h` | P | Supervision header | A4 | n/a | |
| `src/mame/coinline/millennium_audio_trace.cpp` | P | Shared audio JSONL trace writers | A1 | trace parser tests | |
| `src/mame/coinline/millennium_audio_trace.h` | P | Audio trace header | A1 | n/a | |
| `src/mame/coinline/millennium_telephony.cpp` | P | Telephony bridge decode (`audio-routing-state-map`) | A2 | `tests/devices/test_telephony_decode.cpp` (planned) | Feeds route + supervision. |
| `src/mame/coinline/millennium_telephony.h` | P | Telephony header | A2 | n/a | |
| `src/mame/coinline/millennium_security.cpp` | P | Lock/door/vault/service inputs | E5 | `tests/devices/test_security_*.cpp` | |
| `src/mame/coinline/millennium_security.h` | P | Security header | E5 | n/a | |
| `src/mame/coinline/millennium_hostbridge.cpp` | P | Host bridge transport | E6 | `tests/integration/test_host_bridge_*.cpp` | |
| `src/mame/coinline/millennium_hostbridge.h` | P | Host bridge header | E6 | n/a | |
| `src/mame/coinline/millennium_debug.cpp` | P | Trace/symbol/map hooks | E11 | tool tests | |
| `src/mame/coinline/millennium_debug.h` | P | Debug header | E11 | n/a | |

## artwork/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `artwork/README.md` | D | Artwork directory README | docs | n/a | |
| `artwork/millennium-terminal-front.png` | P | Front-panel image | E4 | `tests/integration/test_artwork_*.cpp` | Sourced from `docs/images/terminal-unbranded.webp`. |
| `artwork/millennium.lay` | P | MAME layout | E4/E13b | `tests/integration/test_artwork_*.cpp` | Clickable regions per `artwork-and-layout.md`. |

## roms/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `roms/README.md` | D | Firmware placement instructions | docs | n/a | No firmware committed. |

## fixtures/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `fixtures/README.md` | D | Fixtures index | docs | n/a | |
| `fixtures/board/memory-map.json` | P | Memory map fixture | E1/E2 | `tests/fixtures/test_memory_map_schema.py` | |
| `fixtures/board/io-port-map.json` | P | I/O port map fixture | E1/E2 | `tests/fixtures/test_io_port_map_schema.py` | |
| `fixtures/board/interrupt-map.json` | P | Interrupt map fixture | E1/E3 | `tests/fixtures/test_interrupt_map_schema.py` | |
| `fixtures/board/device-map.json` | P | Device map fixture | E1/E2 | `tests/fixtures/test_device_map_schema.py` | |
| `fixtures/board/board-profile-2line-vfd.json` | P | 2-line VFD profile | E2/E4 | `tests/fixtures/test_board_profile_schema.py` | |
| `fixtures/board/board-profile-11line-vfd.json` | P | 11-line VFD profile | E2/E4 | `tests/fixtures/test_board_profile_schema.py` | |
| `fixtures/firmware/README.md` | P | Firmware metadata only | E1 | n/a | Hashes, no binaries. |
| `fixtures/nvram/factory-default.nvram.json` | P | Factory NVRAM | E7 | `tests/devices/test_nvram_persistence.cpp` | |
| `fixtures/nvram/corrupt-checksum.nvram.json` | P | Corrupt NVRAM | E7 | `tests/devices/test_nvram_corrupt_checksum.cpp` | |
| `fixtures/cards/magcard-valid.json` | P | Valid magstripe | E8 | `tests/devices/test_card_swipe_timing.cpp` | |
| `fixtures/cards/magcard-invalid-lrc.json` | P | Invalid magstripe | E8 | `tests/devices/test_card_lrc_validation.cpp` | |
| `fixtures/cards/smartcard-valid.json` | P | Valid smart card | E8 | `tests/devices/test_smartcard_atr.cpp` | |
| `fixtures/cards/smartcard-empty.json` | P | Empty smart card | E8 | `tests/devices/test_smartcard_atr.cpp` | |
| `fixtures/modem/clean-connect.hex` | P | Clean modem connect | E6 | `tests/devices/test_modem_state_machine.cpp` | |
| `fixtures/modem/dropped-carrier.hex` | P | Carrier loss | E6 | `tests/devices/test_modem_state_machine.cpp` | |
| `fixtures/modem/noisy-line.hex` | P | Noisy line | E6 | `tests/devices/test_modem_state_machine.cpp` | |
| `fixtures/scenarios/boot-to-idle.json` | P | Boot scenario | E10 | `tests/integration/test_boot_to_idle.cpp` | |
| `fixtures/scenarios/keypad-smoke.json` | P | Keypad scenario | E5 | `tests/integration/test_keypad_smoke.cpp` | |
| `fixtures/scenarios/modem-connect.json` | P | Modem connect scenario | E6/E12 | `tests/integration/test_modem_connect.cpp` | |
| `fixtures/scenarios/table-download.json` | P | Table download scenario | E12 | `tests/integration/test_table_download_e2e.cpp` | Create in E12 — scenario file not yet in tree. |
| `fixtures/scenarios/dlog-submit-e2e.json` | P | Optional dedicated DLOG scenario | E12 | `tests/integration/test_dlog_submit_e2e.cpp` | Only if card/coin scenarios insufficient. |
| `fixtures/scenarios/card-call.json` | P | Card call scenario | E8 | `tests/integration/test_card_call.cpp` | |
| `fixtures/scenarios/coin-call.json` | P | Coin call scenario | E9 | `tests/integration/test_coin_call.cpp` | |
| `fixtures/scenarios/service-mode.json` | P | Service mode scenario | E10 | `tests/integration/test_service_mode_entry.cpp` | |
| `fixtures/display/vfd-2line-idle.json` | P | Expected VFD output | E4 | `tests/devices/test_vfd_buffer_snapshot.cpp` | |
| `fixtures/display/vfd-11line-ad.json` | P | Expected 11-line VFD | E4 | `tests/devices/test_vfd_buffer_snapshot.cpp` | |
| `fixtures/board/audio-device-map.json` | P | Audio device port targets | A0 | `tests/fixtures/test_audio_maps_consistency.py` | |
| `fixtures/board/voiceware-command-map.json` | P | Voiceware port decode | A0 | same | |
| `fixtures/board/audio-routing-state-map.json` | P | Routing/mute map | A0 | same | |
| `fixtures/board/disconnect-supervision-map.json` | P | Supervision map | A0 | same | |
| `fixtures/audio/voiceware-command-reset.json` | C | Voiceware reset fixture | A1 | `tests/devices/test_voiceware_*.cpp` | |
| `fixtures/audio/voiceware-play-prompt.json` | C | Voiceware play fixture | A1 | same | |
| `fixtures/audio/voiceware-invalid-prompt.json` | C | Voiceware fault fixture | A1 | same | |
| `fixtures/audio/audio-route-idle.json` | C | Route idle golden | A2 | `tests/devices/test_audio_route_*.cpp` | |
| `fixtures/audio/audio-route-offhook-prompt.json` | C | Off-hook + prompt | A2 | same | |
| `fixtures/audio/audio-route-call-connected.json` | C | Connected route | A2 | same | |
| `fixtures/audio/audio-route-muted.json` | C | Muted route | A2 | same | |
| `fixtures/audio/disconnect-normal.json` | C | Normal disconnect | A4 | `tests/devices/test_supervision_*.cpp` | |
| `fixtures/audio/disconnect-cpc.json` | C | CPC disconnect | A4 | same | |
| `fixtures/audio/disconnect-timeout.json` | C | Timeout disconnect | A4 | same | |
| `fixtures/audio/alerter-error-beep.json` | C | Error cadence | A3 | `tests/devices/test_alerter_*.cpp` | |
| `fixtures/audio/alerter-service-beep.json` | C | Service cadence | A3 | same | |
| `fixtures/scenarios/audio-boot-init.json` | C | Audio boot scenario | A5/A7 | integration tests | |
| `fixtures/scenarios/voice-prompt-playback.json` | C | Voice prompt scenario | A5 | same | |
| `fixtures/scenarios/mic-mute-unmute.json` | C | Mic mute scenario | A5 | same | |
| `fixtures/scenarios/earpiece-mute-unmute.json` | C | Earpiece mute scenario | A5 | same | |
| `fixtures/scenarios/line-to-earpiece-route.json` | C | Line route scenario | A5 | same | |
| `fixtures/scenarios/disconnect-supervision.json` | C | Supervision scenario | A5 | same | |
| `fixtures/scenarios/alerter-output.json` | C | Alerter scenario | A5 | same | |

## tools/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `tools/boot-trace-parser/` | P | Boot trace parser | E11 | tool tests | |
| `tools/io-trace-analyzer/` | P | I/O trace analyzer | E11 | tool tests | |
| `tools/evidence-bundle-export/` | P | Evidence bundle exporter | E11/E13a | `tools/evidence-bundle-export/tests/test_export.cpp` | |

## tests/

| Path | Create/Modify | Purpose | Tranche | Tests | Notes |
| ---- | ------------- | ------- | ------- | ----- | ----- |
| `tests/README.md` | D | Test corpus overview | docs | n/a | |
| `tests/boot/` | P | Boot-tier tests | E2+ | self | |
| `tests/devices/` | P | Per-device tests | E2+ | self | |
| `tests/integration/` | P | Integration tests | E6+ | self | E12: `test_table_download_e2e.cpp`, `test_dlog_submit_e2e.cpp`, `test_table_download_carrier_loss.cpp`; refine `test_modem_connect.cpp`. E13a: `test_scenario_runner_smoke.cpp`, `test_scenario_runner_failure.cpp` (CTest label `scenario`). |
| `tests/fixtures/` | P | Fixture schema tests | E1+ | self | |

## Cross-cutting notes

- Every `*.spec.md` files's schema can be co-published as a JSON schema under `specs/schemas/` once tooling lands (E11/E13).
- The exact MAME driver short-name and ROM region naming are pinned in [`mame-driver-plan.md`](mame-driver-plan.md).
- Updates to this file are mandatory whenever a file is added, renamed, or removed under `coinline-emu/`.
