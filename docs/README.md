# `coinline-emu` documentation index

Public-facing documentation for the CoinLine Terminal Emulator. Internal engineering notes with integration file references are in [`internal/`](internal/) and are excluded from any public navigation.

## Foundation

| Document | Purpose |
| -------- | ------- |
| [architecture.md](architecture.md) | System decomposition, process boundary, device tree. |
| [engine-selection.md](engine-selection.md) | MAME-based vs clean-room comparison; recommendation. |
| [project-structure.md](project-structure.md) | Source-tree, fixtures, artwork, tools, tests layout. |

## Status (measured / rollup)

| Document | Purpose |
| -------- | ------- |
| [status/TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md](status/TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md) | Chip-oriented TP backend: behavioral ROM, hook sequencing, timer note, build paths. |
| [status/implementation-status-audit.md](status/implementation-status-audit.md) | Broad emulator implementation status table. |

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
| [ci-and-release.md](ci-and-release.md) | CI/CD and release artifacts. |

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

## Telephony co-processor (TP 8048)

| Document | Purpose |
| -------- | ------- |
| [tp8048/execution-spec.md](tp8048/execution-spec.md) | Execution and clocking spec for the PCD3349A/8048 backend. |
| [tp8048/asm-state-machine.md](tp8048/asm-state-machine.md) | State-machine notes matching the in-tree behavioural ASM. |
| [tp8048/mame-wiring-notes.md](tp8048/mame-wiring-notes.md) | MAME-side integration notes for the TP backend. |
