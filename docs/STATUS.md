# Project status

**Acceptance snapshot: 27 November 2026.** This page separates machine behavior from component tests, host integration and physical equipment. A record acknowledgment is not proof of an application feature; a successful synthetic transport test is not a live payment or call.

## Terminal families and installation

| Area | Verified | Remaining qualification |
|---|---|---|
| Earlier two-line Multi-Pay | Normal and demo MTR 1.9 cold INSTALL, actual NCC tables, keypad/stripe/coin checks and independent restart | Individual hardware revisions and variants still need their own acceptance |
| Later Multi-Pay | Both tested V1.1 and four tested V1.3 split-flash cases completed INSTALL and warm service | MTR 2.0 case `NCA1X03` is still blocked during required table updates |
| Desk and Card-only | Tested earlier/Desk profiles reached service through the native installation process | Additional variants and card application behavior |
| Coin Basic | Three tested images passed temporary-display INSTALL, restart with display detached and local call | Other hardware configurations |
| Eleven-line graphical | Native graphical controller, 16 exact downloaded bitmap frames, physical softkey input, clock synchronization, cold and retained-state service | Comprehensive production configuration parity |
| Inmate | Profile and candidate catalogued | No runnable image available for native acceptance |

The [firmware acceptance catalog](uat/firmware-acceptance-catalog.md) records 126 identifiers, 85 checked archived files in the original baseline, and 25 of 26 runnable cases with cold INSTALL and in-service restart evidence. The [automated runner](../tools/acceptance/README.md) ties each result to the exact program digest, hardware preset, controller, display, host configuration and retained storage.

For the earlier and later test flows, firmware started with erased EEPROM and blank SRAM. It wrote tables and installation state through emulated hardware and reloaded the resulting storage on a separate launch.

- [Initial end-to-end NCC and INSTALL](status/ncc-install-firmware-stx-handoff-2026-11-23.md)
- [Graphical controller and original profile](status/graphic-vfd-hardware-2026-11-25.md)
- [Full graphical qualification](status/proton-graphical-qualification-2026-11-25.md)
- [Selected downloads and NCC framing audit](status/ncc-audit-remediation-2026-11-26.md)

## Calls, sound and display

| Area | Verified | Remaining qualification |
|---|---|---|
| Line supervision | Native CO detection, answer reversal, handset and remote clearing, New Call behavior | Additional external line interfaces and physical supervision |
| SIP | Real Windows incoming/outgoing media and teardown; actual loopback SIP/RTP and connected DTMF on all three OSes | Physical microphone and external carrier acceptance on Linux/macOS |
| Voiceware | Native ADPCM and stand-alone waveform-element playback; dial, attention and feedback tones, completion interrupt | Unmeasured control/envelope details |
| Two-line / graphical displays | Native VFD rendering and brightness, bitmap transfer, hook-cycle advertising recovery | Additional display modules and host load conditions |
| Regional rating | Earlier native matrix 15/15; later local and national prices, ordinary host quotes and local quarter payment verified after corrections | Later 711/811 service classifications, shortened-number prompt, metered call holding/extension and physical equipment |
| Live ADSI | Independent wire-confirmed CAS/A, CONNECT, welcome, keypad News/Back, softkey Exit, release and normal idle | Longer frames in subsequent LAN tests were interrupted by native PCM underruns; repeat reliability and other application menus remain open |
| Local service routes | Exact direct SIP routing, host quotes for 27 free aliases, genuine graphical short-code calls to two services | Remaining individual native extension trials; ordinary graphical paid-call control still fails |

[Host media and platform support](status/cross-platform-host-media-2026-11-27.md) details Windows WinMM and POSIX PortAudio capture, real SIP loopback tests and package validation. [ADSI navigation](status/connected-dtmf-adsi-welcome-2026-11-25.md), [direct extension routing](status/adsi-direct-lan-routing-2026-11-27.md), [native Voiceware playback](status/voiceware-waveform-2026-11-27.md), [audio reliability](status/host-audio-reliability-2026-11-26.md) and [advertising recovery](status/graphical-ad-hook-resume-2026-11-27.md) contain the more detailed evidence.

## Coin and card behavior

The [regional tariff qualification](status/regional-rating-qualification-2026-11-27.md) records the independent two-line and graphical checks, ordinary NCC rate replies, and remaining service-code gates.

| Area | Verified | Remaining qualification |
|---|---|---|
| Coin validator and escrow | Physical gate motion, coin/reject/refund path, full/jam response and retained box contents | Unmeasured mechanical timing and capacity |
| Cashbox accounting | Native quarter-rated local call, matching record, collection report and empty replacement box | Additional terminal families and reconciliation cases |
| Magnetic stripe | Native swipe/reader checks; a prior local call received a simulated provider authorization and returned to service | Separate ShadyBank end-to-end lab authorization and CDR reconciliation |
| Smart card / modules | Native synchronous contact and persistence tests; generic asynchronous T=0/T=1 transport; distinct internal socket diagnostics | Historical issuer application and protected SAM behavior are not established |
| Virtual card wallet | Native magnetic entry, read and withdrawal indicators; independent contact insert/remove; selection lockout while occupied; primary synthetic contact memory retained across selection and restart | Additional funded synthetic account qualification is independent of reader operation |
| Simulated purse | Independent local nonredeemable ledger and accounting test vectors | Not a historical chip-card payment application |
| Forgotten card | Native reminder display and removal recovery on tested configurations | Audible hardware alarm in the eligible card condition |

Only synthetic instruments are used for payment qualification. A bank reservation is not a captured purchase. The prior ShadyBank integration separately verified one five-cent simulated reservation and its release; native terminal approval, rated connection and settlement are independent gates.

- [Native coin and cashbox evidence](status/coin-cashbox-qualification-2026-11-26.md)
- [Card and payment qualification](status/smartcard-contact-2026-11-25.md)
- [Native contact transport](status/smartcard-native-transport-2026-11-25.md)
- [Simulated purse model](lab-purse.md)

## Firmware storage and recovery

The later-board model includes two persistent NOR flash chips, common SRAM and resident bootstrap address mapping. A synthetic CPU program erases/programs each flash device and verifies neighboring sectors and persistence after a separate restart. Installed eligible firmware has picked up a scheduled MT94 job and dialed the independent code-server route; a complete actual remote image transfer, validation, reboot and interrupted-transfer recovery remain open.

[Remote firmware download and storage](firmware-download-storage.md) records the model and qualification boundaries. [Peripheral handoff evidence](status/peripheral-handoff-implementation-2026-11-25.md) covers other board signals, enclosure warning and data-jack relay probes; external data-modem sessions remain unqualified.

## Builds and verification

Native Windows x64, Linux x64 and macOS Apple Silicon builds are available through [GitHub Actions](https://github.com/andrew867/CoinLine-emu/actions/workflows/native-release.yml). The public verification suite includes native C++ tests, fixture validation and loopback SIP/media tests. Linux and macOS have native PortAudio capture-adapter tests using a deterministic device fixture. Physical equipment and provider interoperability are tested separately.

- [Build and package](../BUILDING.md)
- [Tests and evidence](../TESTING.md)
- [CI and release procedure](ci-and-release.md)
- [Hardware reference requests](../specs/hardware/hardware-reference-request-checklist.md)
