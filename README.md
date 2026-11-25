# CoinLine Terminal Emulator

CoinLine runs the original Millennium-compatible terminal firmware on a Z180 MAME driver. It includes the terminal front panel, emulated handset and peripherals, and a local central-office simulator. CoinLine Server supplies provisioning and rating over the firmware’s NCC modem connection.

## Start here

- [Windows quick start](RUNNING.md) for the portable ZIP, controls, first INSTALL, and SIP.
- [Build and package](BUILDING.md) for Windows x64, Ubuntu Linux x64, macOS Apple Silicon, and the operator ROM input layout.
- [Current verification](docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md) for the firmware and network evidence.
- [Firmware acceptance catalog](docs/uat/firmware-acceptance-catalog.md) for the tested terminal families, archive hashes and outstanding gates.
- [Automated INSTALL runner](tools/acceptance/README.md) for cold installation, hardware selection and restart testing.
- [Remote firmware downloads](docs/firmware-download-storage.md) for the MTR 2.x bootstrap, flash model and upgrade acceptance plan.
- [Testing](TESTING.md) for the emulator suite and Windows smoke checks.

## Current status

Both the MTR 1.9 demo and normal OEM firmware completed INSTALL from erased EEPROM and blank SRAM. Each accepted all ten NCC table records, passed CO and answer supervision and the keypad, Track 2, and coin tests, then rebooted to **Please lift receiver** from firmware-written battery storage.

The demo dialed the toll-free test without requesting a rate and reached National Directory Assistance through SignalWire. The OEM displayed a live five-cent, one-minute CoinLine quote and played the matching Voiceware prompt. The OEM speaks cents in five-cent increments. These results cover the tested MAME configurations and lab table set. Physical terminal certification is a separate validation activity.

The acceptance library covers 126 firmware identifiers and 85 archived
artifacts with verified SHA-256 digests. Twenty-three of the 24 runnable
firmware identifiers have completed cold INSTALL and an in-service restart
across the recorded builds. Hardware presets support earlier
Multi-Pay, Desk and Card-only terminals, later V1.0, V1.1 and V1.3 Multi-Pay
control boards, and Coin Basic. The catalog records successful firmware-driven
INSTALL and restart outcomes separately from table acknowledgements and
individual peripheral checks. Coin Basic uses a different display and
Voiceware port mapping, with a temporary VFD connection selectable at launch.
All three tested Coin Basic images completed detached-display Craft access,
INSTALL with the temporary display attached, and a locally answered call
after removing the display and restarting. The Inmate firmware is still
missing a usable program image.

Later boards now model two persistent NOR flash chips and the resident
bootstrap address remap. Remote code download acceptance requires the
terminal's actual code-server exchange, flash writes, verification and reboot.
The emulator flash device and the resident bootstrap have independent tests.
A synthetic Z180 program executes from SRAM, programs both flash chips through
the memory bus, and verifies the saved bytes after a separate launch. The
original firmware inputs remain unchanged. Actual remote upgrades and recovery
after interrupted delivery still need a compatible code-server exchange.

The Windows build supports SIP audio and selection of a recording device. An OEM inbound call through SignalWire carried captured audio to the calling side and cleared on remote hangup. Linux and macOS builds use the local central-office simulator. SIP audio and microphone capture on those platforms are future work.

The emulator and the MIT-licensed CoinLine Server and Management Console communicate over the NCC transport. Authorized firmware and Voiceware images are local inputs. Public artifacts contain the emulator and original telephony model. See [license strategy](LICENSE-STRATEGY.md) and the [CoinLine Server repository](https://github.com/andrew867/CoinLine).

[Documentation index](docs/README.md) · [Device specifications](specs/) · [Test plans](test-plans/)
