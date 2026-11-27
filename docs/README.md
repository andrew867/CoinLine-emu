# CoinLine emulator documentation

CoinLine is a firmware-driven terminal emulator, not a screen simulation. This documentation is organized by what you want to do, with measured acceptance kept separate from the original engineering plans.

## Start here

| Goal | Guide |
|---|---|
| Run a terminal, perform INSTALL, place calls and use its controls | [Operating guide](../RUNNING.md) |
| Build a Windows, Linux or macOS package | [Build and package](../BUILDING.md) |
| See what actually works and what remains unqualified | [Current acceptance status](STATUS.md) |
| Reproduce device and firmware tests | [Testing](../TESTING.md), [automated INSTALL runner](../tools/acceptance/README.md) |
| Understand how the emulator and server fit together | [Architecture](architecture.md), [project structure](project-structure.md) |

## Acceptance and troubleshooting

- [Firmware acceptance catalog](uat/firmware-acceptance-catalog.md): exact program candidates, hardware presets, cold INSTALL, restart and unresolved cases.
- [Current status](STATUS.md): feature-level evidence and known limits, with links to dated reports.
- [Debugging guide](debugging-guide.md), [evidence bundles](evidence-bundles.md) and [scenario runner](scenario-runner.md): collect reproducible test results.
- [CI and release](ci-and-release.md): how native builds, checksums and source archives are produced.
- [Outstanding hardware reference checklist](../specs/hardware/hardware-reference-request-checklist.md): questions that still require additional hardware documentation or measurement.

## Hardware reference

### Control boards and processor

[Board profiles](board-profiles.md) · [Device model](device-model.md) · [Memory map](memory-map.md) · [I/O port map](io-port-map.md) · [Interrupt map](interrupt-map.md) · [Z180 core](z180-core.md) · [Z180 internal peripherals](z180-internal-peripherals.md)

### Display and human interface

[VFD emulation](vfd-emulation.md) · [Keypad and quick-access keys](keypad-emulation.md) · [Handset and hook switch](hookswitch-and-handset.md) · [Art and layout](artwork-and-layout.md) · [Front panel](frontend-control-panel.md)

### Line and sound

[Modem and UART host bridge](modem-uart-host-bridge.md) · [Line disconnect supervision](disconnect-supervision-emulation.md) · [Voiceware](voiceware-emulation.md) · [Audio routing](audio-routing-emulation.md) · [Alerter audio](alerter-audio.md) · [Host media](status/cross-platform-host-media-2026-11-27.md)

### Coin, cards and security

[Coin validator](coin-validator-emulation.md) · [Magnetic stripe reader](card-reader-emulation.md) · [Smart cards](smartcard-emulation.md) · [Local simulated purse](lab-purse.md) · [Lock, housing and cashbox](lock-door-vault-service.md)

### Storage and management

[NVRAM and table storage](nvram-and-table-storage.md) · [Table download behavior](table-download-behavior.md) · [Remote firmware download and recovery](firmware-download-storage.md)

## Engineering evidence

The [current status](STATUS.md) links the major verified results. The [status directory](status/) preserves individual dated reports, including:

- [Cold INSTALL, persistence and host rating](status/ncc-install-firmware-stx-handoff-2026-11-23.md)
- [Graphical advertising and display](status/graphic-vfd-hardware-2026-11-25.md), [hook-cycle recovery](status/graphical-ad-hook-resume-2026-11-27.md)
- [Native ADSI and connected DTMF](status/connected-dtmf-adsi-welcome-2026-11-25.md), [direct service routing](status/adsi-direct-lan-routing-2026-11-27.md)
- [Card contact and payment](status/smartcard-contact-2026-11-25.md), [native T=0/T=1](status/smartcard-native-transport-2026-11-25.md), [full graphical qualification](status/proton-graphical-qualification-2026-11-25.md)
- [Coin and cashbox](status/coin-cashbox-qualification-2026-11-26.md), [peripheral signals](status/peripheral-handoff-implementation-2026-11-25.md)
- [Voiceware waveform synthesis](status/voiceware-waveform-2026-11-27.md), [host audio reliability](status/host-audio-reliability-2026-11-26.md), [window timing](status/windows-window-timing-2026-11-25.md)

## Design and implementation history

Historical plans describe the decisions and milestones at the time they were written. They are not a substitute for the current acceptance status.

- Foundation: [engine selection](engine-selection.md), [MAME driver plan](mame-driver-plan.md), [license strategy](../LICENSE-STRATEGY.md), [Windows MSYS2 build](building-on-windows-msys2.md).
- Implementation: [roadmap](implementation-roadmap.md), [tranches](tranche-plan.md), [file plan](file-plan.md), [build plan](build-plan.md), [boot milestones](boot-milestones.md), [host integration](host-integration-plan.md), [risk register](risk-register.md).
- Verification: [test strategy](test-strategy.md), [acceptance plan](acceptance-test-plan.md), [compatibility validation items](compatibility-validation-items.md).
- Audio design: [MAME device architecture](audio-mame-device-architecture.md), [boot integration](audio-boot-integration-plan.md), [implementation roadmap](audio-implementation-roadmap.md), [debugging and traces](audio-debugging-and-trace-plan.md), [CI](audio-ci-plan.md), [evidence bundle plan](audio-evidence-bundle-plan.md).

The source, device specifications and test plans remain available in [src](../src/), [specs](../specs/) and [test-plans](../test-plans/).
