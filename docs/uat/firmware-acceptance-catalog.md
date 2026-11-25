# Millennium firmware acceptance catalog

Snapshot date is 2026-11-24. The catalog joins 122 wiki entries with 22 `mm_manager` compatibility entries by firmware ID. Two IDs come only from the manager matrix and two more from the dump archives, for a total of 126. The public collections supplied 85 files for 25 firmware IDs, including 66 distinct byte streams. The count of files includes companion images and extracted data. [Firmware versions wiki](https://wiki.muc.ccc.de/millennium:firmwareversions) · [Pinned manager compatibility matrix](https://github.com/hharte/mm_manager/blob/a5b595f6189245885bcc2a7232681a11061ac71c/README.md#compatibility)

The manager column records external hardware reports. CoinLine acceptance applies to the exact firmware, control and telephony board, display, Voiceware, emulator build, and host configuration tested together. A downloaded image is a candidate. Editor support, transfer acknowledgement, and observed terminal behavior are recorded as separate results.

## Current acceptance

Twenty-three of the 24 staged IDs with usable program candidates have a verified
cold INSTALL and separate in-service restart. This includes all four tested
V1.3 and both V1.1 split-flash images, both Desk hardware revisions, earlier
Multi-Pay and Card-only cases, all three staged Coin Basic images, and the
local FreeTel demo. The Coin Basic cases also passed the detached portable
display preparation, attached-display INSTALL and restart, and detached-display
local call and hangup sequence. The original V1.1 and V1.3 chip-card tests
failed. A subsequent pin-level synchronous card model passed NLA1X05's
complete cold INSTALL and in-service restart. The other card tests still
require independent reruns on this new model.
MTR 2.0 `NCA1X03` remains blocked during required table updates. The other
102 catalogue IDs have missing dumps or unresolved program
layout.

The archive counts above exclude the additional locally supplied NLA1X08
MTR 2.12-based graphical/ADSI Multi-Pay candidate (terminal type 40,
internal DLA version 1.3). Its mapped 1 MiB program SHA-256 is
`112800558509bab98f939ea9a52f1710bd3d77085617d55612d21a1eabfc4874`.
The native 11-line profile with later-board DMA1 wiring completed cold
INSTALL, including all 18 NCC records, keypad, magnetic-card and escrow
checks. The chip-card check failed. All 16 downloaded MT131/132 bitmap
screens were confirmed as exact 112 by 72 pixel matches in the native VFD.
It also returned to service on an independent warm launch after a transient
telephony alarm and normal NCC maintenance. Including this additional
local candidate, 24 of 25 runnable IDs have verified cold INSTALL and
in-service restart, with the chip-card limitation noted. Graphical menus
and ADSI service operation remain independent qualification gates.
[Graphical hardware evidence](../status/graphic-vfd-hardware-2026-11-25.md)

The updated synchronous reader also passed NLA1X08's standalone Craft
contact test twice. It did not establish a funded payment application.
[Card contact evidence](../status/smartcard-contact-2026-11-25.md)

## Local firmware library

The library is under `build/firmware-acceptance/`. Look in `images/<firmware ID>/` for the unchanged bytes and in `inventory.json` for roles, sizes, SHA-256 hashes, Git blob IDs, and pinned per-file links. `SHA256SUMS` verifies all 85 staged files from the library root with `sha256sum --check SHA256SUMS`. The same local directory holds the raw wiki snapshot and Git objects used for the import. The workspace ignores the library and the release builder uses its own input paths. [Pinned primary dump archive](https://github.com/muccc/millennium/tree/a70a19ec891c65e4d70e2ce5c833ce9b517889bf/firmware) · [Pinned supplementary dump archive](https://github.com/armeniki/Nortel-Millennium/tree/2abc8f42910b05e42a2870947c47d8f58a8df915/Firmware)

The archive contains CPU images, flash halves, Voiceware, Voiceware fragments, display prompt data, and programmable logic dumps. The manifest records each role separately. Identical hashes across sources are corroboration of bytes. Different hashes under the same identifier are separate candidates. Keep archive files intact and use isolated run directories for assembled inputs and terminal state.

## Initial qualification order

| Order | Firmware | Purpose and required input |
| --- | --- | --- |
| Baseline | `NBA1F02`, MTR 1.9 Multi-Pay | The public U5 image exactly matches the raw OEM input used by the local package. Replay with the validated MTR 1.9 board, Voiceware, and NCC lab table set. The existing lab run completed first INSTALL, restored battery-backed state at restart, and observed real-time rating. |
| 1 | `NKA1X02`, MTR 2.11 Multi-Pay | The archive supplies U2 and X1 program flash and both Voiceware chips. Confirm the V1.1 board mapping and flash order, then test cold boot, INSTALL, warm restart, and free and rated calls. Record the firmware's requested table inventory before choosing the profile. |
| 2 | `NBJ1S03`, MTR 1.9 Desk | Both archive copies have the same SHA-256. Use the Desk control and key-card workflow, confirm the display and reader, and test the relevant call paths. Record the matching Voiceware separately if available. The prior external hardware report gives a bench comparison. |
| 3 | `NPE1S01`, MTR 1.20 Coin Basic | Exercise coin-only INSTALL, denomination and credit handling, answer supervision, and collect and refund behavior. Record the supported display and audio configuration. |
| 4 | `NCA1X03`, MTR 2.0 Multi-Pay | Both flash halves are staged. Confirm the equipment and Voiceware combination, then repeat the Multi-Pay gates. |
| 5 | `NQA1X01`, Multi-Pay | The wiki and image filenames say 2.12, and the manager matrix says 2.20. Use the exact archived U2 and X1 pair and record the version from the terminal's status message. |
| 6 | `NLA1X05`, `NOA1X04`, `NOK1X03`, `NNA1F02` | Extend coverage to the 2.12 Multi-Pay profile and the staged 2.12, 2.13, and 1.9 TTY variants after establishing a Multi-Pay and Desk reference run. |

The `NBA1F02` baseline CPU image SHA-256 is `8273d9eb5d8799a8e1b6ccdf3d98da7d5e7973f9016e44aa1c4b878beeeadc8f`. The local MTR 1.9 demo is a separate regression configuration. Its completed INSTALL, persistence, and no-rate free call apply to that lab image. The first automated sweep also verified the public FreeTel image's local INSTALL and restart. [MTR 1.9 lab acceptance evidence](../status/ncc-install-firmware-stx-handoff-2026-11-23.md)

## Conflicting or incomplete records

- `NQA1X01`, `PBAXS05`, and `NKA1X05` retain both reported MTR versions. `PBAXS05` has a 227,722-byte artifact with the expected firmware ID string, plus an unsuccessful external hardware report. Its image format and full memory layout remain unresolved.
- `NBJ1F03` is identified inside the supplementary archive image. The wiki separately lists `NBF1F03`. Keep both records and use a separate run if an `NBF1F03` image is located.
- The wiki describes `NAD4S02` as display-equipped. The supplementary archive places the image in a no-display folder and the manager calls it Coin Basic. Identify the correct hardware at the bench before setting display expectations.
- `06JAA03` has conflicting language descriptions. `M06CAA17` and `M06CBBX1` are the archive IDs for manager rows `06CAA17` and `06CBBX1`.
- `NPA1S01` has two different program images under one ID. The manager links to the second dump. The supplementary archive matches that second program image.
- `NLA1X06` is listed as private. `PBAXS03` and `PBAXS04` are listed as awaiting preparation. No public image was located in the reviewed sources for those rows or for the `M06KEC12` Inmate build.

## Acceptance procedure

The [batch runner](../../tools/acceptance/README.md) verifies the local library, runs the terminal's INSTALL prompts, saves cold and warm outcomes, and records board mapping, input hashes, and NCC evidence for each attempt.

On the emulator, create a new battery and configuration directory for each cold test. Start with erased EEPROM and blank SRAM and let the firmware initialize and install them through its normal workflow. Capture the ROM and Voiceware hashes, board and display profile, emulator commit and package checksums, server commit, table-set version and record hashes, and run timestamp in UTC. For a physical run, also record the terminal hardware and control board revision. Use an isolated local central office and NCC record set with synthetic card and call destinations. [Emulator acceptance plan](../acceptance-test-plan.md)

The portable launcher defaults to the OEM MTR 1.9 image and selects the hardware preset from the input hash and program tag. An explicit `--hardware` option supports alternate boards and independent battery files. The driver supports `COINLINE_BOARD` and `COINLINE_MOTHERBOARD` for the board profile and wiring. It accepts a single program image through `COINLINE_FIRMWARE`, or two inputs through `COINLINE_FIRMWARE_FLASH0` and `COINLINE_FIRMWARE_FLASH1` in verified bank order. Record both component hashes. [Firmware loader](../../src/mame/coinline/millennium_state.cpp)

| Gate | Pass evidence |
| --- | --- |
| Boot | Observe reset and first usable VFD or model-specific indicator. Record the board profile, firmware identity, last milestone, and alarm status. |
| Craft and line | Enter model-appropriate Craft, complete line and answer-supervision tests, and observe lift and hangup detection. For Desk, capture the key-card access path. |
| Download | Save the NCC session, time sync, install request, transmitted record IDs and hashes, application ACKs, end-of-data, and final clear. Record a missing required record or timeout at the specific gate. |
| Terminal self-test | Exercise applicable keypad, card, coin, housing, and lock inputs through the firmware's prompts. Confirm `INSTALL` completes and the terminal enters service. |
| Restart | Restart using the battery state written by the same run. Confirm return to service and re-use of the downloaded configuration. |
| Table behavior | Verify the visible or audible result of each feature under test. Capture the applied value, the triggering input, and its observed effect. Record individual table ACKs independently. |
| Calls | Verify free-call outpulsing without a rating exchange. For a paid test, verify the returned amount against the display and spoken prompt. Record supervision, disconnect, and coin disposition or synthetic card result where applicable. |

Use `pass`, `fail`, `blocked`, `unrun`, or `not applicable` per gate. Include the reason for each blocked or nonapplicable result. Keep emulator evidence and physical evidence in distinct run rows. For table coverage, log `editor available`, `ACK observed`, and `behavior observed` as independent fields keyed by table ID, payload hash, and run ID.

## First automated sweep

The 2026-11-24 sweep recorded 126 catalog outcomes, verified all 85 staged BINs, and attempted 26 distinct program cases across 24 firmware IDs. The two emulator motherboard mappings produced 33 attempts. The 102 blocked catalog entries lacked a usable program image in the current library. The full summary, exact hashes, and per-attempt evidence are under `build/firmware-acceptance/runs/20261124T152214.674484Z/`.

| Firmware | Measured result |
| --- | --- |
| `NBA1F02` stock and date-format variants | Both passed cold INSTALL, each with 12 table ACKs, and returned to service from the saved battery image. |
| `NNA1F02` | Passed the standard INSTALL and restart with 12 table ACKs and the shared lab Voiceware pair. TTY-specific I/O is outside this result. |
| `FREETEL-1.x` | Passed local INSTALL and returned to service after restart. The capture contains zero NCC frames. |
| `M06CAA17`, `M06CBBX1` | Passed Craft and line supervision, acknowledged seven tables, then displayed status 24 after the recorded start of the MT92 screening record. |
| `NKA1X02`, `NCA1X03`, `NQA1X01` | Reached the initial Z180 register snapshot on both mappings without reaching an INSTALL display. The initial interrupt and DMA register snapshots matched the MTR 1.9 baseline values at that checkpoint. |
| `NPA1S01` and `NPA1F01` | The three program images passed the Craft banner and line checks on retest. Each sent MT44, received MT18, and reported status 24 before any table ACK. |
| `NBJ1S03`, `NBJ1F03` | Both accepted the key card after Craft PIN entry and reached the CO line check. Both timed out there. |
| `NAJ2S05` | Passed Desk Craft and line supervision on retest. It acknowledged seven tables before reporting download status 24. |

The follow-up MTR 1.20 results are under `build/firmware-acceptance/runs/20261124T160824.102374Z/`. The Desk results are under `build/firmware-acceptance/runs/20261124T161536.865322Z/`.

## Hardware presets and dedicated NCC targets

The acceptance runner selects a preset from the verified program inventory.
The launcher also supports explicit selection and isolates each mapped program's
battery image. The presets use the two implemented motherboard I/O maps.
The firmware tag identifies the image's board designation.

CoinLine has 13 primary isolated lab targets mapped to 23 firmware IDs,
independent V1.1 and V1.3 verification targets, and a separate MTR 2.0
archived-defaults diagnostic target. The host
assigns an individual published table set to each target's wire ID. Eight
primary targets have a matching entry in the host family catalogue. Five use explicit
firmware inventory entries and targeted table sets pending family catalogue
coverage. The local provisioning report retains record hashes and assignments.

| Firmware | Result with the dedicated target |
| --- | --- |
| `NPA1S01` | The first run acknowledged ten records and stopped at MT101. The second program image received MT136 in the corrected expanded set, acknowledged all 11 records, completed INSTALL, and restored the installed battery state on restart. |
| `NPA1F01` | Acknowledged all 11 expanded records. An initial run timed out during the post-download dialogue. A fresh run completed INSTALL and restored service after restart. |
| `M06CAA17` | Acknowledged all ten records in the legacy set, including MT24, completed INSTALL, and restored the installed battery state on restart. |
| `M06CBBX1` | Acknowledged all nine records in the Card-only set, completed INSTALL, and restored the installed battery state on restart. |
| `NAJ2S05` | Acknowledged all nine records in the initial Desk set and reported status 32, an invalid terminal type. With the card terminal type in MT26, it completed the keypad and card checks, exited INSTALL with `#`, and restored service after restart. |
| `NBJ1S03` | An initial dedicated-target trial accepted the Craft key card and stalled at the off-hook prompt. A second telephony configuration transfer reached 22 bytes after the command header before outgoing controller traffic stopped. Later trials completed both line checks. |
| `NKA1X02` | The V1.1 preset selected the later map. The first trial reached Voiceware port writes. After correcting the later board's ROM and I/O decoding, a fresh probe reached `Not installed / Use # to INSTALL`. |

The MTR 1.20 and MSR 1.7 Multi-Pay evidence is in
`build/firmware-acceptance/runs/20261124T164731.492546Z/`.
The Desk evidence is in `build/firmware-acceptance/runs/20261124T165754.415451Z/`.
The MTR 1.20 French retest and Desk REL 1 controller trace are in
`build/firmware-acceptance/runs/20261124T170549.778666Z/` and
`build/firmware-acceptance/runs/20261124T171126.043580Z/`.
The completed Desk REL 2 retest is in `build/firmware-acceptance/runs/20261124T171357.666482Z/`.
The completed Card-only retest is in `build/firmware-acceptance/runs/20261124T171904.309244Z/`.
The completed French MTR 1.20 retest is in `build/firmware-acceptance/runs/20261124T181706.178193Z/`.
The V1.1 register and I/O captures are in `build/firmware-acceptance/runs/20261124T163918.727135Z/`
and `build/firmware-acceptance/runs/20261124T165017.237538Z/`.

## Additional INSTALL and boot results

| Firmware | Highest verified gate | Evidence |
| --- | --- | --- |
| `NAA1S05`, `06JAA03`, `12AAV08` | Cold INSTALL and in-service restart | Ten application record ACKs per image, including MT24 and MT74. |
| `NNK1F05` | Cold INSTALL and in-service restart | Eleven application record ACKs. The local CO missed a hook event on the first attempt. The retry passed. TTY data I/O remains untested. |
| `NCA1X03`, `NKA1X02`, `NLA1X05`, `NKA1X05`, `NQA1X01`, `NOK1X03`, `NOA1X04` | Native INSTALL entry and CO line check | All seven advanced from the startup menu to `Checking C.O. connection` with the housing open at cold start and time for the telephony controller to settle. This comparison ended at status 42, during modem dial-tone detection. |
| `NKA1X02`, `NLA1X05` | NCC download, 11 record ACKs each | The corrected later-board modem serial path and DTMF source selection passed the CO line and answer-supervision checks. Both firmware images completed the provisional expanded transfer and reported status 34, still-required table updates. The revised expanded recipe includes MT72. |
| `NLA1X05`, `NQA1X01`, `NOK1X03`, `NOA1X04` | Download complete and keypad check | A revised 12-record table set including MT72 advanced all four V1.3 images to the card test. The first NLA1X05 reader correction removed the blockage indication but the track read still failed. The hardware presets now select DMA1 for the V1.1 and V1.3 magnetic reader clock. |
| `NLA1X05`, `NQA1X01`, `NOK1X03`, `NOA1X04` | Magnetic reader check | All four accepted the synthetic magnetic track using DMA1. The earlier harness inserted a magnetic card again at the smart-card prompt. After the firmware offered to continue, each reached coin-validator initialization and reported status 61. The runner now distinguishes magnetic and smart-card input and records failures independently. |
| `NKA1X02`, `NKA1X05` | NCC download, 12 record ACKs each | Both V1.1 images returned status 34 after the revised set. Readback of the `NKA1X02` battery showed the downloaded records and their stored flags, including MT50 and MT72. The next test exercises the firmware's own retry prompt. |
| `NCA1X03` | NCC download, 11 record ACKs | The MTR 2.0 image passed line and answer supervision and acknowledged its universal records, then returned status 34. The saved battery contains the downloaded records and stored flags. Its record inventory does not require MT72. |
| `NCA1X03`, `NKA1X02` | Firmware-managed download retry | The later-board serial UART build passed both line checks and acknowledged the entire first set. Each reported status 34, accepted the retry input, began a second download and subsequently reported status 24. The first MT44 requested cash-box status but the host returned MT18 alone. |
| `NLA1X05` | Magnetic reader check and smart-card prompt | The serial UART build received validator responses and completed the 12-record download. After the magnetic track check it reached the actual smart-card input, but the firmware continued prompting for insertion. |
| `NLA1X05`, `NQA1X01` | Coin calibration prompt | Connecting the shared card contacts let the smart-card test run and report its failure explicitly. The firmware continued to coin calibration without status 61. The first coin timed out with status 65. The UART showed repeated escrow refunds and status polls with a stationary position sensor. A timed travel state now reports movement before the gate returns to its resting position. |
| `NLA1X05`, `NQA1X01` | Coin calibration and escrow completed 00 | With the timed escrow sensor, both accepted the two calibration coins and the escrow test coin, then reported completion after refund and collection. The driver reached its 40-second peripheral deadline six frames after the final prompt and exited before pressing `*`. The peripheral deadline now allows both tests to complete. |
| `NLA1X05`, `NOA1X04` | Final EEPROM verification | Both completed the coin test and accepted `*`, then reported status 71. The `NOA1X04` battery had all 512 EEPROM bytes erased. A later-board pin trace confirmed ten command bits after START. The previous decoder consumed an extra bit, preventing completed writes. The corrected model decodes six- or eight-bit word addresses by board revision. |
| `NLA1X05`, `NQA1X01`, `NOK1X03`, `NOA1X04` | Cold INSTALL and in-service restart | With the corrected EEPROM decoder each completed all 12 records, keypad, magnetic card, calibration, refund and collection. Their chip-card tests reported failure and the firmware explicitly allowed continuation. Each displayed `Please lift receiver`, then did so again on a separate launch. All four final battery EEPROM arrays contained 156 bytes changed from erased state. |
| `NCA1X03`, `NKA1X02` | Required table update remains | With the corrected EEPROM decoder the V1.0 and V1.1 firmware wrote 146 and 156 EEPROM bytes from erased state respectively, acknowledged their first assigned sets, and still requested further updates with status 34. The subsequent calls failed with status 21 and 24 respectively. |
| `NCA1X03`, `NKA1X02` | Cash-box status reply diagnostic | A loopback test relay appended the documented empty MT38 to MT18 when requested. The subsequent MT44 reason was `0x02`, establishing that the cash-box request cleared. Both still reported required updates. |
| `NKA1X02`, `NKA1X05` | V1.1 table geometry | Both exact program images mark MT85 and MT86 mandatory, at 8,001 bytes each. The dedicated V1.1 lab set now includes the two public default records alongside the expanded 12-record recipe. All 14 stored hashes and validation results were verified against the published set. |
| `NKA1X02` | Download and magnetic reader completed | The 14-record set resolved status 34. The firmware passed the keypad and magnetic reader tests, then timed out at calibration. The UART trace records an interrupt-handler status read with both the interrupt and valid UART status bits clear, followed by UART reinitialization and an additional refund. |
| `NKA1X05` | Cold INSTALL and in-service restart | With the 14-record V1.1 set the firmware acknowledged every table, passed the keypad, magnetic reader and escrow tests, and entered service on both launches. The chip-card check failed. |
| `NKA1X02`, `NKA1X05` | Cold INSTALL and in-service restart | Both V1.1 images passed on the corrected Z180 interrupt-sampling build. Each acknowledged all 14 records, passed keypad, magnetic reader, calibration and escrow, and returned to service on a separate launch. Each EEPROM contained 156 bytes changed from erased state. No invalid UART line-status reads occurred at the observed interrupt-handler instruction. Chip-card tests failed independently. |
| `NCA1X03` | Required-update condition | A read-only watch recorded all ten mandatory header flags as `0x80` at the physical reads immediately before status 34, including the two paged records. This independently confirms the stored flags were visible to the processor. The follow-up download did not complete. |
| `NCA1X03` | Complete first and follow-up downloads | With maskable interrupt sampling corrected, all eleven records were acknowledged in each of two complete NCC calls. Status 34 remained after both. |
| `NCA1X03` | Archived-defaults diagnostic | On a separate target the firmware acknowledged all 24 compatible default and essential records, including MT29, MT33, MT55, MT58, and MT102 through MT110. The host transmitted MT13. All ten watched mandatory flags were `0x80` at the final reads. The terminal reported status 23 before completing the download dialogue. The 11-record primary target remains assigned. |
| `NBA1F02`, `NLA1X05` | Interrupt sampling regression | Both MTR 1.9 program candidates and the V1.3 reference completed cold INSTALL and warm restart on the corrected Z180 core. The V1.3 UART trace showed zero interrupt-handler LSR reads with the UART IRQ deasserted and no valid status bits, compared with three in its preceding passing run. |
| `NOK1X03` | Coin calibration prompt | Passed the keypad, magnetic reader and validator initialization, then timed out waiting for the first calibration coin. The workstation test driver now allows acceptance to settle and one repeat insertion per unchanged prompt. |
| `NBJ1S03` | Cold INSTALL and in-service restart | The terminal ACKed ten records and completed the Desk keypad and card checks through a transparent loopback capture relay and again over a direct NCC connection. Earlier direct attempts cleared during clock sync. |
| `NBJ1F03` | CO line, answer supervision and NCC clock reply | Completed the key-card Craft path and both line checks. The terminal ACKed MT20 and sent NAK plus clear while the NCC was ending clock sync. Status 24 appeared before MT44 or any table ACK. |
| `NBJ1F03` | NCC download completed 01 | A subsequent run ACKed its ten records, then reported that the telephony board was not responding before the keypad test. A further diagnostic attempt cleared during download with status 24. |
| `NBJ1F03` | Cold INSTALL and in-service restart | The French Desk REL 1 image passed Craft, both line checks, all ten table ACKs, keypad and magnetic card checks, and returned to service on restart. |
| `NPE1S01`, `NAD4K02`, `NAD4S02` | Voiceware commands and telephony processor handshake | All three reached the M7B telephony receive checkpoint on the earlier map. Neither map produced a stable display. A model-appropriate service indicator or line behavior still needs verification for the no-display variant. |
| `NPE1S01`, `NAD4K02`, `NAD4S02` | Cold INSTALL and in-service restart with VFD attached | Captures established that these images send display bytes to I/O `0xC0`, whereas the other earlier-board images use `0xA0`. Coin Basic now routes the display and Voiceware to the appropriate exchanged ports. All three completed keypad and coin tests, INSTALL, and a separate in-service restart. The MTR 1.20 image acknowledged ten records, and both MTR 1.7 images acknowledged nine. |
| `NPE1S01`, `NAD4K02`, `NAD4S02` | Full portable-display workflow and local call | All three completed detached Craft access, a power cycle to attach the portable display, firmware-driven INSTALL, attached-display restart to service, and a separate detached-display call that was answered and hung up through the local CO. |

The legacy INSTALL and restart results are in `build/firmware-acceptance/runs/20261124T182658.858280Z/`.
The split-flash boot results are in `build/firmware-acceptance/runs/20261124T183624.319068Z/`
and `build/firmware-acceptance/runs/20261124T184456.772422Z/`.
The subsequent later-board INSTALL entry probes are in
`build/firmware-acceptance/runs/20261124T192623.783664Z/` and
`build/firmware-acceptance/runs/20261124T192719.394414Z/`.
The first later-board NCC transfers are in `build/firmware-acceptance/runs/20261124T193758.151556Z/`.
The revised V1.3 and V1.1 table runs are in
`build/firmware-acceptance/runs/20261124T195010.215036Z/` and
`build/firmware-acceptance/runs/20261124T195100.311417Z/`.
The V1.3 magnetic reader and coin-validator results are in
`build/firmware-acceptance/runs/20261124T202246.965898Z/` and
`build/firmware-acceptance/runs/20261124T202425.604682Z/`.
The corrected serial UART trials are in
`build/firmware-acceptance/runs/20261124T204906.519931Z/`,
`build/firmware-acceptance/runs/20261124T204906.402673Z/`, and
`build/firmware-acceptance/runs/20261124T205425.355254Z/`.
The earlier-board regression passed cold INSTALL and warm restart in
`build/firmware-acceptance/runs/20261124T204907.931321Z/`.
The coin calibration trials are in
`build/firmware-acceptance/runs/20261124T210408.331453Z/` and
`build/firmware-acceptance/runs/20261124T210411.953625Z/`.
The completed escrow tests are in
`build/firmware-acceptance/runs/20261124T211604.206504Z/` and
`build/firmware-acceptance/runs/20261124T211606.679043Z/`.
The escrow model uses a nominal two-second gate travel time within the four-second
cycle window. Physical gate timing remains unmeasured.
The later-board EEPROM failures are in
`build/firmware-acceptance/runs/20261124T212525.863903Z/` and
`build/firmware-acceptance/runs/20261124T213728.824144Z/`.
The pin-level capture confirming the later EEPROM command length is in
`build/firmware-acceptance/runs/20261124T214040.081737Z/`.
The completed V1.3 cold INSTALL and separate warm restarts are in
`build/firmware-acceptance/runs/20261124T214625.416204Z/` and
`build/firmware-acceptance/runs/20261124T214623.943800Z/`.
The V1.0 and V1.1 trials after the EEPROM fix are in
`build/firmware-acceptance/runs/20261124T215454.973684Z/` and
`build/firmware-acceptance/runs/20261124T214657.865432Z/`.
The cash-box protocol diagnostic is in
`build/firmware-acceptance/cashbox-reply-diagnostic/`, with runs
`20261124T220447.068576Z` and `20261124T220448.232925Z`.
The 14-record V1.1 download and coin diagnostic are in
`build/firmware-acceptance/runs/20261124T221706.813298Z/`.
The MTR 2.0 physical read watch is in
`build/firmware-acceptance/runs/20261124T222031.956736Z/`.
The interrupt sampling regression runs are
`build/firmware-acceptance/runs/20261124T223428.689893Z/`,
`build/firmware-acceptance/runs/20261124T223430.106053Z/`, and
`build/firmware-acceptance/runs/20261124T223433.456669Z/`.
Both V1.1 cold INSTALL and restart passes on the corrected interrupt core are in
`build/firmware-acceptance/runs/20261124T223954.738009Z/` and
`build/firmware-acceptance/runs/20261124T224034.726114Z/`.
The MTR 2.0 archived-defaults test is in
`build/firmware-acceptance/runs/20261124T225132.583481Z/`.
Both MTR 1.9 candidates passed again on that same EEPROM decoder build in
`build/firmware-acceptance/runs/20261124T214627.158092Z/`.
The shared-reader build passed both MTR 1.9 program candidates through cold
INSTALL and warm restart in `build/firmware-acceptance/runs/20261124T210409.824796Z/`.
The Desk retests are in `build/firmware-acceptance/runs/20261124T190510.389720Z/`.
The successful Desk run and its wire capture are in
`build/firmware-acceptance/desk-ncc-wire.zqaiyljs/`.
The successful direct Desk retest is in `build/firmware-acceptance/runs/20261124T191804.450205Z/`.
The French Desk download and telephony failure are in
`build/firmware-acceptance/runs/20261124T212113.896457Z/` and
`build/firmware-acceptance/runs/20261124T212803.746275Z/`.
The completed French Desk INSTALL and restart are in
`build/firmware-acceptance/runs/20261124T224334.339398Z/`.
The Coin Basic probes are in `build/firmware-acceptance/runs/20261124T185318.863788Z/`.
The MTR 1.20 Coin Basic portable-display Craft trial in
`build/firmware-acceptance/runs/20261124T210731.334429Z/`
timed out without a display prompt after entering its access code and PIN and
opening the housing. The terminal craft interface guide, Issue 00.01, Appendix C,
pp. C-12 to C-16, describes attaching the portable VFD to J7 and entering Craft with the handset on-hook.
The same guide, pp. C-1 to C-18, confirms that Coin Basic without a permanent
display uses an internal temporary module while the Inmate terminal normally
uses an external portable module. Both procedures enter Craft blind, unlock
the terminal, disconnect power, attach the display to J7, and restore power.
The Coin Basic module is removed before returning the terminal to service.
The Inmate housing cannot close over the external display cable, so the
locking tiebar is operated during maintenance. A separate power-cycle test
with portable-display connection and removal remains to be qualified.
The handset-cycle and portable-display retries for all three Coin Basic images
are in `build/firmware-acceptance/runs/20261124T211610.830695Z/`.
All three timed out after the Craft input and housing opening. The `NPE1S01`
telephony trace included keypad response messages during the access and PIN input.
The portable display remained blank, so neither INSTALL nor service is verified.
The additional `NPE1S01` I/O capture in
`build/firmware-acceptance/runs/20261124T214927.200199Z/`
recorded no VFD-tagged accesses, despite the firmware containing the Craft prompts.
The corrected interrupt-core retest also timed out before the first usable
display in `build/firmware-acceptance/runs/20261124T223431.895557Z/`.
The corrected Coin Basic display-map passes are in
`build/firmware-acceptance/runs/20261125T001143.534536Z/` and
`build/firmware-acceptance/runs/20261125T001144.670711Z/`.
The complete portable-display and local-call runs are under
`build/firmware-acceptance/runs/20261125T002828.482620Z/`
for MTR 1.20 and
`build/firmware-acceptance/runs/20261125T003507.616429Z/`
for both MTR 1.7 images.
The later-board bootstrap and NOR model boot probe reached the startup
display in `build/firmware-acceptance/runs/20261125T001146.165708Z/`.
The NLA1X05 cold INSTALL passed with twelve record ACKs in
`build/firmware-acceptance/runs/20261125T001336.179620Z/`.
Its first warm launch timed out at the old 40-second prompt deadline.
A separate launch of its retained state reached service after the firmware's
bootstrap and maintenance dialogue, in
`build/firmware-acceptance/warm-bootstrap-probe/20261125T002610.235858Z/`.
The later warm-start deadline now allows 120 emulated seconds.
After correcting the reset-time latch, the V1.3 reference `NLA1X05`
completed cold INSTALL with all twelve records and returned to service
with both its battery and initial program flash restored in
`build/firmware-acceptance/runs/20261125T004254.220778Z/`.
The flash image's active hash and both chip hashes remained unchanged
through this ordinary INSTALL and restart.
Both V1.1 images reached their initial display using the corrected
bootstrap, then accepted all fourteen records in the first full retest.
Both reached the peripheral tests before the 600-second wall deadline.
The runner now allows 1200 seconds by default for full acceptance.
The extended retests completed INSTALL and returned to service on both
images in `build/firmware-acceptance/runs/20261125T005216.733789Z/`
for `NKA1X02` and
`build/firmware-acceptance/runs/20261125T005215.631502Z/`
for `NKA1X05`. Each accepted all fourteen records and reported battery and
program flash restoration on the separate warm launch. Their active and
per-chip flash hashes were unchanged across the ordinary INSTALL and restart.
The MTR 2.0 bootstrap retest still acknowledged all eleven records in each
of two calls and reported status 34 in
`build/firmware-acceptance/runs/20261125T001335.925140Z/`.
The corrected reset-latch build reproduced the same two complete eleven-record
exchanges followed by status 34 in
`build/firmware-acceptance/runs/20261125T004738.480200Z/`.
The Inmate candidate `M06KEC12` remains blocked because no runnable image
was located in the reviewed public archives.
The explicit inventory-only check is in
`build/firmware-acceptance/inmate-input-check/20261124T235639.561882Z/`.
The MTR 1.9 Multi-Pay regression passed cold INSTALL and warm restart for both
program images in `build/firmware-acceptance/runs/20261124T190234.144509Z/`.
A separate later-board restart loaded a copy of a battery from an older build,
exposed the correct flash, and saved a battery with zero low RAM overlay entries.

## Firmware inventory

The numeric type is the wiki's terminal-type code. A dash indicates a field absent from the cited source. `Staged` counts all local artifacts for that ID, including companion images. `No dump located` describes the reviewed sources. The last column records the highest verified emulator gate for at least one program candidate, with detailed runs above.

| Firmware ID | Wiki MTR | Type | Catalog description | Manager MTR | Manager hardware report | Local dump | Emulator acceptance |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `M12ABX10` | 1.6s | 12 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `M12ABX11` | 1.6s | 12 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `M12ABX12` | 1.6 | 9 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `M06CAA10` | 1.7 | 13 | Multipay Multicard E/S | - | - | No dump located | Input missing |
| `M06CAA17` | 1.7 | 13 | Multipay Multicard E/S | 1.7 | MP/MC, Partial, chip cards and mandatory-table alarm | Staged (1) | Cold INSTALL + restart |
| `M06CAF03` | 1.7 | 13 | Multipay Magstripe E/S | - | - | No dump located | Input missing |
| `M06CBBX1` | 1.? | - | Multipay Card-Only | 1.7 | Card-only, English / Spanish, Working except chip cards | Staged (1) | Cold INSTALL + restart |
| `M06CFA01` | 1.7 | 14 | Multipay Multicard E/S | - | - | No dump located | Input missing |
| `M06KFA01` | 1.7 | 14 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `NAA1F03` | 1.7 | 26 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `NAA1F04` | 1.7 | 26 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `NAA1J03` | 1.7 | 23-26 | Multipay Multicard E/J | - | - | No dump located | Input missing |
| `NAA1J04` | 1.7 | 26 | Multipay Multicard E/J | - | - | No dump located | Input missing |
| `NAA1J06` | 1.7 | 26 | Multipay Multicard E/J | - | - | No dump located | Input missing |
| `NAA1S03` | 1.7 | 26 | Multipay Multicard E/S | - | - | No dump located | Input missing |
| `NAA1S04` | 1.7 | 26 | Multipay Multicard E/S | - | - | No dump located | Input missing |
| `NAA1S05` | 1.7 | 26 | Multipay Multicard E/S | 1.7 | MP/MC, Unconfirmed | Staged (1) | Cold INSTALL + restart |
| `NAA1S06` | 1.7 | 26 | Multipay Multicard E/S | - | - | No dump located | Input missing |
| `NAE1F02` | 1.7 | 23 | Card only multicard E/F | - | - | No dump located | Input missing |
| `NAE1J02` | 1.7 | 23 | Card only multicard E/J | - | - | No dump located | Input missing |
| `NAE1S02` | 1.7 | 23 | Card only multicard E/S | - | - | No dump located | Input missing |
| `MO6KDB02` | 1.7 | 15 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `MO6KDB03` | 1.7 | 15 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `MO6KDB05` | 1.7 | 15 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `MO6KDB07` | 1.7 | 15 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `MO6KDC01` | 1.7 | 15 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `MO6KDC02` | 1.7 | 15 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `MO6CDB01` | 1.7 | 15 | Desk Multicard E/S | - | - | No dump located | Input missing |
| `MO6CDB02` | 1.7 | 15 | Desk Multicard E/S | - | - | No dump located | Input missing |
| `NAJ1S06` | 1.7 | 25 | Desk Multicard E/S | - | - | No dump located | Input missing |
| `NAJ1S07` | 1.7 | 25 | Desk Multicard E/S | - | - | No dump located | Input missing |
| `NAJ1F06` | 1.7 | 25 | Desk Multicard E/F | - | - | No dump located | Input missing |
| `NAJ1J04` | 1.7 | 25 | Desk Multicard E/J | - | - | No dump located | Input missing |
| `NAJ1J06` | 1.7 | 25 | Desk Multicard E/J | - | - | No dump located | Input missing |
| `NAJ1J07` | 1.7 | 25 | Desk Multicard E/J | - | - | No dump located | Input missing |
| `NAJ1S04` | 1.7 | 25 | Desk Multicard E/S | - | - | No dump located | Input missing |
| `NAJ2J01` | 1.7 | 25 | Desk Multicard E/J REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2J02` | 1.7 | 25 | Desk Multicard E/J REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2S01` | 1.7 | 25 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2S03` | 1.7 | 25 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2S04` | 1.7 | 25 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2S05` | 1.7 | 25 | Desk Multicard E/S REL 2 H/W | 1.7 | Desk, Working except chip cards | Staged (4) | Cold INSTALL + restart |
| `NAJ2J03` | 1.7 | 25 | Desk Multicard E/J REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2S06` | 1.7 | 25 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NAJ2S07` | 1.7 | 25 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NAD4J01` | 1.7 | 19 | Coin Only Display E/J | - | - | No dump located | Input missing |
| `NAD4J02` | 1.7 | 19 | Coin Only Display E/J | - | - | No dump located | Input missing |
| `NAD4S02` | 1.7 | 19 | Coin Only Display E/S | 1.7 | Coin Basic, Unconfirmed | Staged (1) | Cold INSTALL, restart, detached VFD local call |
| `NAD4S03` | 1.7 | 19 | Coin Only Display E/S | - | - | No dump located | Input missing |
| `NADXS01` | 1.7 | 19, 24 | Coin Only No Display E/S | - | - | No dump located | Input missing |
| `NADXS02` | 1.7 | 19 | Coin Only No Display E/S | - | - | No dump located | Input missing |
| `NADXS03` | 1.7 | 19 | Coin Only No Display E/S | - | - | No dump located | Input missing |
| `NADXS04` | 1.7 | 19 | Coin Only No Display E/S | - | - | No dump located | Input missing |
| `M06KOB02` | 1.7 | - | Card Only NETAS | - | - | No dump located | Input missing |
| `M06KOB04` | 1.7 | - | Card Only NETAS | - | - | No dump located | Input missing |
| `M06CND04` | 1.7 | - | Card Only MACEDONIA/NETAS | - | - | No dump located | Input missing |
| `M06JOB02` | 1.7 | - | Card Only JORAN/NETAS | - | - | No dump located | Input missing |
| `M06CNB02` | 1.7 | - | Card Only BOSNIA/NETAS | - | - | No dump located | Input missing |
| `M06KEC12` | 1.7i | 15 | Inmate E/F | - | - | No dump located | Input missing |
| `NBA1F01` | 1.9 | 30 (28) | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `NBA1F02` | 1.9 | 30 (28) | Multipay Multicard E/F | 1.9 | MP/MC, English / French, Working | Staged (4) | Both images installed + restarted |
| `NBA1F03` | 1.9 | 30 | Multipay Multicard E/F | - | - | No dump located | Input missing |
| `NBA1J01` | 1.9 | 30 | Multipay Multicard E/J | - | - | No dump located | Input missing |
| `NBE1F01` | 1.9 | 27 | Card only multicard E/F | - | - | No dump located | Input missing |
| `NBE1S01` | 1.9 | 27 | Card only multicard E/S | - | - | No dump located | Input missing |
| `NBJ1F01` | 1.9 | 29 | Desk Multicard E/F REL 1 H/W | - | - | No dump located | Input missing |
| `NBF1F03` | 1.9 | 29 | Desk Multicard E/F REL 1 H/W | - | - | No dump located | Input missing |
| `NBJ1J03` | 1.9 | 29 | Desk Multicard E/J REL 1 H/W | - | - | No dump located | Input missing |
| `NBJ1S03` | 1.9 | - | Desk Multicard | 1.9 | Desk, English / Spanish, Working | Staged (2) | Cold INSTALL + restart |
| `NBJ2F01` | 1.9 | 29 | Desk Multicard E/F REL 2 H/W | - | - | No dump located | Input missing |
| `NBJ2F03` | 1.9 | 29 | Desk Multicard E/F REL 2 H/W | - | - | No dump located | Input missing |
| `NBJ2F04` | 1.9 | 29 | Desk Multicard E/F REL 2 H/W | - | - | No dump located | Input missing |
| `NBJ2J03` | 1.9 | 29 | Desk Multicard E/J REL 2 H/W | - | - | No dump located | Input missing |
| `NBJ2J04` | 1.9 | 29 | Desk Multicard E/J REL 2 H/W | - | - | No dump located | Input missing |
| `PBAXS02` | 1.9 | 32 | Multipay PERU | - | - | No dump located | Input missing |
| `PBAXS03` | 1.9 | 32 | Multipay PERU | - | - | Pending dump | Input missing |
| `PBAXS04` | 1.9 | 32 | Multipay PERU | - | - | Pending dump | Input missing |
| `PBAXS05` | 1.9 | - | Multipay PERU | 1.7 | MP/MC/International, Unsuccessful | Staged (1) | Image layout unresolved |
| `EBA1S01` | 1.9 | 32 | Multipay EQUADOR | - | - | No dump located | Input missing |
| `IBAXS01` | 1.9 | 46 | Multiplay INTERNATIONAL | - | - | No dump located | Input missing |
| `IBDXS01` | 1.9 | 42 | Coin Only INTERNATIONAL | - | - | No dump located | Input missing |
| `IBF4S01` | 1.9 | 33 | Card Only COLOMBIA | - | - | No dump located | Input missing |
| `IBK4S01` | 1.9 | 33 | Deskset COLOMBIA | - | - | No dump located | Input missing |
| `NNA1F02` | 1.9 | - | Multipay TTY E/F | 1.9 | MP/MC/TTY, English / French, Working | Staged (2) | Cold INSTALL + restart |
| `NJJXS01` | 1.11 | 45 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NJJXS02` | 1.11 | 45 | Desk Multicard E/S REL 2 H/W | - | - | No dump located | Input missing |
| `NNK1F05` | 1.13 | - | Multipay Magstripe TTY | 1.13 | MP/MC/TTY, English / French, Working | Staged (2) | Cold INSTALL + restart |
| `12AAV08` | 1.xx | - | Multipay Multicard E/F | 1.7 | MP/MC, Unconfirmed | Staged (2) | Cold INSTALL + restart |
| `06JAA03` | 1.xx | - | Multipay Magstripe E/S | 1.7 | MP/MC, English / Japanese, Working except chip cards | Staged (2) | Cold INSTALL + restart |
| `NPA1S01` | 1.20 | - | Multipay Multicard E/S | 1.20 | MP/MC, English / Spanish, Working | Staged (7) | Cold INSTALL + restart |
| `NPE1S01` | 1.20 | - | Multipay Coin Basic | 1.20 | Coin Basic, Working | Staged (1) | Cold INSTALL, restart, detached VFD local call |
| `NPA1F01` | 1.20 | - | Multipay | - | - | Staged (2) | Cold INSTALL + restart |
| `NCA1S01` | 2.0 | 36 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NCA1S02` | 2.0 | 36 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NCA1X01` | 2.0 | 36 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NCA1X02` | 2.0 | 36 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NCA1X03` | 2.0 | 36 | Multipay Multicard Downloadable | 2.0 | MP/MC/DJ, Working | Staged (2) | 11 records ACKed, status 34. 24-record diagnostic, status 23. |
| `NED2X01` | 2.0 | 38 | Coin Only Display | - | - | No dump located | Input missing |
| `NED4X01` | 2.0 | 38 | Coin Only No Display | - | - | No dump located | Input missing |
| `NCE2X01` | 2.0 | 36 | Card MAC WEBSTN (NetVenue) | - | - | No dump located | Input missing |
| `NCE2X02` | 2.0 | 36 | Card MAC WEBSTN (NetVenue) | - | - | No dump located | Input missing |
| `NCE2X03` | 2.0 | 36 | Card NETVENUE | - | - | No dump located | Input missing |
| `NCE3X01` | 2.0 | 36 | Card NETVENUE | - | - | No dump located | Input missing |
| `NEA1X02` | 2.0 | 40 | Multipay Graphic | - | - | No dump located | Input missing |
| `NEA1S01` | 2.2 | 22 | Multipay Graphic E/S | - | - | No dump located | Input missing |
| `NEA3F01` | 2.2 | 22 | Multipay Mondex | - | - | No dump located | Input missing |
| `NEA3F02` | 2.2 | 22 | Multipay Mondex | - | - | No dump located | Input missing |
| `NEA3F03` | 2.2 | 22 | Multipay Mondex | - | - | No dump located | Input missing |
| `NEA3F04` | 2.2 | 22 | Multipay Mondex | - | - | No dump located | Input missing |
| `NFA1S02` | 2.3 | 34 | Multipay E-Purse | - | - | No dump located | Input missing |
| `NKA1X01` | 2.11 | 51 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NKA2X01` | 2.11 | 53 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NKA2X02` | 2.11 | 53 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NKA1X02` | 2.11 | 51 | Multipay Multicard Downloadable | 2.11 | MP/MC, English / Spanish, Working | Staged (4) | Cold INSTALL + restart, chip-card failure |
| `NKA1X03` | 2.11 | 51 | Multipay Multicard Downloadable | - | - | No dump located | Input missing |
| `NKD1X04` | 2.11 | 56 | Coin Only Display | - | - | No dump located | Input missing |
| `NLA1X05` | 2.12 | - | Multipay Multicard | 2.12 | Unconfirmed | Staged (2) | Cold INSTALL + restart, chip-card contact passed |
| `NLA1X06` | 2.12 | - | Multipay Multicard Demo | - | - | Private listing | Input missing |
| `NOA1X04` | 2.12 | - | Multipay Multicard Dialpad-TTY | - | - | Staged (4) | Cold INSTALL + restart, chip-card failure |
| `NQA1X01` | 2.12 | - | MP/MC | 2.20 | MP/MC, English / Spanish, Working | Staged (4) | Cold INSTALL + restart, chip-card failure |
| `NOK1X03` | 2.13 | - | Multipay Multicard TTY | 2.13 | MP/MC/TTY, Unconfirmed | Staged (4) | Cold INSTALL + restart, chip-card failure |
| `NKA1X05` | 2.xx | unknown | Multipay Multicard | 2.11 | MP/MC, Unconfirmed | Staged (27) | Cold INSTALL + restart, chip-card failure |
| `NBE1J01` | - | - | Card-only | 1.9 | Card-only, English / Japanese, Provisioned, rating failed | No dump located | Input missing |
| `NAD4K02` | - | - | Coin Basic | 1.7 | Coin Basic, Unconfirmed | Staged (1) | Cold INSTALL, restart, detached VFD local call |
| `NBJ1F03` | - | - | Desk archive image | - | - | Staged (1) | Cold INSTALL + restart |
| `FREETEL-1.x` | - | - | FreeTel demo | - | - | Staged (1) | Cold INSTALL + restart (local) |
