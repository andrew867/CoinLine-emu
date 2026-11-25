# CoinLine Terminal Emulator

CoinLine runs the original Millennium-compatible terminal firmware on a Z180 MAME driver. It includes the terminal front panel, emulated handset and peripherals, and a local central-office simulator. CoinLine Server supplies provisioning and rating over the firmware’s NCC modem connection.

## Start here

- [Windows quick start](RUNNING.md) for the portable ZIP, controls, first INSTALL, and SIP.
- Separate OEM two-line and graphical 11-line launchers provide independent terminal storage and SIP setup in the same package.
- [Build and package](BUILDING.md) for Windows x64, Ubuntu Linux x64, macOS Apple Silicon, and the operator ROM input layout.
- [INSTALL verification](docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md) for the firmware and network evidence.
- [Card contact and payment qualification](docs/status/smartcard-contact-2026-11-25.md), [ADSI interoperation](docs/status/adsi-interop-2026-11-25.md), and [graphical performance](docs/status/graphical-render-performance-2026-11-25.md) for the latest live trials and remaining gates.
- [Native smart-card transport and high-DPI window sizing](docs/status/smartcard-native-transport-2026-11-25.md) for the synthetic ASCI0 and memory-persistence tests.
- [Interactive and audio milestone status](docs/status/interactive-audio-milestone-status.md) for the native New Call relay and SIP-dialog restart check.
- [Windows window and artwork timing](docs/status/windows-window-timing-2026-11-25.md) for nonmodal window moves/resizes and native renderer measurements.
- [Connected DTMF and ADSI navigation qualification](docs/status/connected-dtmf-adsi-welcome-2026-11-25.md) for live menu navigation and SIP transport verification.
- [Firmware acceptance catalog](docs/uat/firmware-acceptance-catalog.md) for the tested terminal families, archive hashes and outstanding gates.
- [Automated INSTALL runner](tools/acceptance/README.md) for cold installation, hardware selection and restart testing.
- [Remote firmware downloads](docs/firmware-download-storage.md) for the MTR 2.x bootstrap, flash model and upgrade acceptance plan.
- [Outstanding hardware reference checklist](specs/hardware/hardware-reference-request-checklist.md) for remaining documentation and physical validation priorities.
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
The optional 11-line graphical display has a separate four-page pixel
controller and physical softkey scanner. Its synthetic native hardware
probe and artwork presentation pass. The locally supplied NLA1X08
MTR 2.12-based graphical/ADSI image (terminal type 40, DLA 1.3)
completed cold INSTALL and a separate in-service restart, acknowledged
all 18 NCC records, and displayed all 16 downloaded bitmap-advertisement
frames exactly. Its keypad, magnetic-card and coin tests passed. A later
pin-level smart-card implementation passed the installed firmware's Craft
contact test twice, following the original failed INSTALL chip-card check.
A separate MTR 2.12 NLA1X05 cold INSTALL and restart also passed with
the new reader model. A subsequent magnetic-card trial obtained a simulated
host authorization and connected a local test call. A CDR and payment
settlement were not observed. The synchronous contact fixture is nonfunded;
a funded chip application and a fresh NLA1X08 INSTALL with the new reader
remain separate gates.

The graphical board now uses its 24.576 MHz oscillator and the local ADSI
route carries real PCMU audio over SIP/RTP. Live firmware trials produced
the capability A tone, decoded authentic CONNECT frames and emitted D1
responses, independently confirmed by a wire capture. After offloading the
services, a native call completed the first D within the one-second deadline,
acknowledged the welcome screen and rendered its menu on the physical VFD model.
With the service returning to voice between screens, the native keypad selected
News and Back, the native softkey selected Exit, and the call released cleanly.
Paid service calls remain a separate gate.

A deterministic single CONNECT test produced native D onset in 791.587 ms.
The live receiver now shares that dedicated emulated 8 kHz sampling clock;
the latest live result used the unchanged hardware clocks and service deadline.
Ordinary connected-call keypad DTMF now follows the controller tone gate and
uses negotiated RTP events, supported SIP INFO, or in-band PCM as appropriate.

The enclosure warning source and gate now follow the later hardware latch,
with a separate legacy Desk oscillator profile. Native GPIO and recorded
audio measurements verified the later waveform and silent muted phases.
The optional later-board data-jack monitor and raw relay combinations also
passed a native bus probe; external modem audio and data-session acceptance
are still pending. See [peripheral qualification](docs/status/peripheral-handoff-implementation-2026-11-25.md).
All three tested Coin Basic images completed detached-display Craft access,
INSTALL with the temporary display attached, and a locally answered call
after removing the display and restarting. The Inmate firmware is still
missing a usable program image.

Later boards now model two persistent NOR flash chips and the resident
bootstrap address remap. Remote code download acceptance requires the
terminal's actual code-server exchange, flash writes, verification and reboot.
Both MTR 2.11 V1.1 candidates and the MTR 2.12 V1.3 reference completed
normal INSTALL and restart through the restored bootstrap, with their
battery and initial flash restored.
The emulator flash device and the resident bootstrap have independent tests.
A synthetic Z180 program executes from SRAM, erases and programs a sector
on each flash chip through the memory bus, checks neighboring sectors,
and verifies the saved bytes after a separate launch. The
original firmware inputs remain unchanged. Actual remote upgrades and recovery
after interrupted delivery still need a compatible code-server exchange. The
modem offers a configurable, independent loopback route for the firmware's
code-server call.

The Windows build supports SIP audio and selection of a recording device. An OEM inbound call through SignalWire carried captured audio to the calling side and cleared on remote hangup. Linux and macOS builds use the local central-office simulator. SIP audio and microphone capture on those platforms are future work.

The emulator and the MIT-licensed CoinLine Server and Management Console communicate over the NCC transport. Authorized firmware and Voiceware images are local inputs. Public artifacts contain the emulator and original telephony model. See [license strategy](LICENSE-STRATEGY.md) and the [CoinLine Server repository](https://github.com/andrew867/CoinLine).

[Documentation index](docs/README.md) · [Device specifications](specs/) · [Test plans](test-plans/)
