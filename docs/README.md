# `coinline-emu` documentation index

Documentation for the CoinLine Terminal Emulator. Start with the [Windows quick start](../RUNNING.md), [build and package command](../BUILDING.md), and [current verification](status/ncc-install-firmware-stx-handoff-2026-11-23.md). Dated status reports and planning documents record their original scope.

## Foundation

| Document | Purpose |
| -------- | ------- |
| [architecture.md](architecture.md) | System decomposition, process boundary, device tree. |
| [engine-selection.md](engine-selection.md) | MAME-based vs clean-room comparison; recommendation. |
| [license-isolation.md](license-isolation.md) | How GPL/MIT isolation is enforced in practice. |
| [project-structure.md](project-structure.md) | Source-tree, fixtures, artwork, tools, tests layout. |

## Status (measured / rollup)

| Document | Purpose |
| -------- | ------- |
| [Windows operator guide](../tools/windows/portable/quick-start.txt) | Contents of the Quick Start included in each portable ZIP. |
| [status/ncc-install-firmware-stx-handoff-2026-11-23.md](status/ncc-install-firmware-stx-handoff-2026-11-23.md) | Current end-to-end INSTALL, battery persistence, outbound SIP, and OEM voice-rate evidence. |
| [status/TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md](status/TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md) | Chip-oriented TP backend: behavioral ROM, hook sequencing, timer note, build paths. |
| [status/implementation-status-audit.md](status/implementation-status-audit.md) | Historical bring-up audit and trace references. |

## Planning

| Document | Purpose |
| -------- | ------- |
| [implementation-roadmap.md](implementation-roadmap.md) | High-level path from E0 to E13. |
| [tranche-plan.md](tranche-plan.md) | Per-tranche goals, files, tests, acceptance, exit. |
| [file-plan.md](file-plan.md) | Master table of every planned file. |
| [build-plan.md](build-plan.md) | Build-system rollout plan. |
| [test-strategy.md](test-strategy.md) | Global test approach. |
| [acceptance-test-plan.md](acceptance-test-plan.md) | Acceptance gate definition. |
| [boot-milestones.md](boot-milestones.md) | M0–M12 milestone ladder. |
| [debugging-guide.md](debugging-guide.md) | Debug workflows, traces, evidence. |
| [risk-register.md](risk-register.md) | Risks, mitigations, tests, tranches. |
| [compatibility-validation-items.md](compatibility-validation-items.md) | Compatibility items with explicit code-vs-validation distinction. |
| [host-integration-plan.md](host-integration-plan.md) | Path to a CoinLine Host integration. |
| [evidence-bundles.md](evidence-bundles.md) | Evidence bundle contents and exporter. |
| [scenario-runner.md](scenario-runner.md) | Scenario verb set and JSON schema. |
| [ci-and-release.md](ci-and-release.md) | CI/CD, release artifacts, sanitizer checks. |

## Platform and CPU

| Document | Purpose |
| -------- | ------- |
| [mame-driver-plan.md](mame-driver-plan.md) | MAME machine driver layout and registration. |
| [board-profiles.md](board-profiles.md) | 2-line vs 11-line VFD and per-revision profiles. |
| [memory-map.md](memory-map.md) | ROM, RAM, NVRAM, table storage, vectors, stack, heap. |
| [io-port-map.md](io-port-map.md) | Known/suspected/unknown ports + unknown-port policy. |
| [interrupt-map.md](interrupt-map.md) | Vector ordering, sources, masks, priorities. |
| [z180-core.md](z180-core.md) | Z180 CPU expectations and core integration. |
| [z180-internal-peripherals.md](z180-internal-peripherals.md) | MMU, ASCI, PRT, INTs, DMA, wait states. |
| [device-model.md](device-model.md) | Common device contract (reset, I/O, tick, ints, snapshot, trace, fixtures). |

## Devices

| Document | Purpose |
| -------- | ------- |
| [vfd-emulation.md](vfd-emulation.md) | 2-line and 11-line VFD device. |
| [keypad-emulation.md](keypad-emulation.md) | Numeric keypad, quick-access keys, volume, language. |
| [hookswitch-and-handset.md](hookswitch-and-handset.md) | Hookswitch, handset audio loopback, volume. |
| [card-reader-emulation.md](card-reader-emulation.md) | Magnetic card reader timing and LRC. |
| [smartcard-emulation.md](smartcard-emulation.md) | Smart-card reset/ATR/APDU or memory-card behavior. |
| [coin-validator-emulation.md](coin-validator-emulation.md) | Coin pulse trains, denominations, errors. |
| [alerter-audio.md](alerter-audio.md) | Alerter / call-progress audio. |
| [voiceware-emulation.md](voiceware-emulation.md) | Voice playback device (prompts / tones). |
| [audio-routing-emulation.md](audio-routing-emulation.md) | Telephony processor routing commands. |
| [disconnect-supervision-emulation.md](disconnect-supervision-emulation.md) | Line supervision injection model. |
| [lock-door-vault-service.md](lock-door-vault-service.md) | Lock, door, vault, service-mode inputs. |
| [modem-uart-host-bridge.md](modem-uart-host-bridge.md) | ASCI/UART, modem signals, host bridge. |
| [nvram-and-table-storage.md](nvram-and-table-storage.md) | NVRAM image format, factory/corrupt images, table storage. |
| [firmware-download-storage.md](firmware-download-storage.md) | DLA staging area and verification. |
| [table-download-behavior.md](table-download-behavior.md) | Table download flows and persistence. |

## Frontend

| Document | Purpose |
| -------- | ------- |
| [artwork-and-layout.md](artwork-and-layout.md) | MAME `.lay` plan + clickable regions. |
| [frontend-control-panel.md](frontend-control-panel.md) | Optional control-panel UI overlay. |

## Internal-only (integration traceability)

| Document | Purpose |
| -------- | ------- |
| [internal/README.md](internal/README.md) | INTERNAL-ONLY banner and index. |
| [internal/firmware-evidence-inventory.md](internal/firmware-evidence-inventory.md) | Inventory of evidence sources. |

The full internal index is at [`internal/README.md`](internal/README.md). Internal documents are not part of any public site navigation.
