# CoinLine Terminal Emulator

CoinLine runs the original Millennium-compatible terminal firmware on a Z180 MAME driver. It includes the terminal front panel, emulated handset and peripherals, and a local central-office simulator. CoinLine Server supplies provisioning and rating over the firmware’s NCC modem connection.

## Start here

- [Windows quick start](RUNNING.md) for the portable ZIP, controls, first INSTALL, and SIP.
- [Build and package](BUILDING.md) for Windows x64, Ubuntu Linux x64, macOS Apple Silicon, and the operator ROM input layout.
- [Current verification](docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md) for the firmware and network evidence.
- [Testing](TESTING.md) for the emulator suite and Windows smoke checks.

## Current status

Both the MTR 1.9 demo and normal OEM firmware completed INSTALL from erased EEPROM and blank SRAM. Each accepted all ten NCC table records, passed CO and answer supervision and the keypad, Track 2, and coin tests, then rebooted to **Please lift receiver** from firmware-written battery storage.

The demo dialed the toll-free test without requesting a rate and reached National Directory Assistance through SignalWire. The OEM displayed a live five-cent, one-minute CoinLine quote and played the matching Voiceware prompt. The OEM speaks cents in five-cent increments. These results cover the tested MAME configurations and lab table set. Physical terminal certification is a separate validation activity.

The Windows build supports SIP audio and selection of a recording device. An OEM inbound call through SignalWire carried captured audio to the calling side and cleared on remote hangup. Linux and macOS builds use the local central-office simulator. SIP audio and microphone capture on those platforms are future work.

The emulator and the MIT-licensed CoinLine Server and Management Console communicate over the NCC transport. Authorized firmware and Voiceware images are local inputs. Public artifacts contain the emulator and original telephony model. See [license strategy](LICENSE-STRATEGY.md) and the [CoinLine Server repository](https://github.com/andrew867/CoinLine).

[Documentation index](docs/README.md) · [Device specifications](specs/) · [Test plans](test-plans/)
