# Outstanding hardware reference checklist

This is the collection checklist for documentation and physical observations
needed to finish hardware-faithful terminal emulation. The linked technical
handoffs describe the current implementation and acceptance tests. A checked
reference item means the requested evidence has been collected and reviewed;
it does not mean native firmware acceptance has passed.

The six peripheral handoffs have been received and reviewed, including the
correlated service-manual references. They establish enough register-level
behavior to begin implementation. Circuit-level timing, connector pinouts
and identification of particular components remain separate requests below.

See the [reference evidence review](hardware-reference-evidence-review.md) for the
original product/service publication page and figure references already located,
including legacy alert circuitry, daughterboards, the portable VFD and Desk
accessories. Partial reference evidence does not close a schematic or pinout item.

## Priority 1: missing hardware details

| Reference requested | Current evidence or remaining uncertainty | What it should unblock |
| --- | --- | --- |
| [ ] Forgotten-card alarm schematic, transducer and source-selection circuit, oscillator and cadence controls for each revision | Later-board warning waveform, gate and source-select signals identified; the shared card-power latch signal and earlier-board routing require physical confirmation. | Genuine card-left reminder, cancellation, ring restoration and distinct enclosure/handset audio. [Handoff](forgotten-card-alarm-black-box-handoff.md) |
| [ ] External data-jack schematic, connector pinout, R4/R5 relay and monitor truth table | Preliminary relay model exists; external modem attachment, current sensing, line transfer and teardown are not yet natively qualified. | A physical or virtual external modem establishes and disconnects a real data session without mis-signalling the handset or internal modem. [Handoff](data-jack-black-box-handoff.md) |
| [ ] Supported contact-card and reader part numbers, datasheets, contact/switch timing and electrical specifications | Synchronous contact tests passed on reference firmware; asynchronous UART transport, internal module selection and application-specific card programming remain unqualified. | Correct reader reset, clock, data, supply and slot isolation across supported revisions; application tests with authorized, nonfunded fixtures. [Handoff](smartcard-contact-black-box-handoff.md) |
| [ ] ADSI CAS/FSK modem and analogue-path schematics, chipset identification, carrier detect/release and mute/relay specifications | Actual CAS/A and CONNECT/D1 are observed, but the first completed D still misses the service deadline. Firmware first/retry timer branches have been measured; analogue timing and transport contributions are not completely isolated. | A calibrated physical modem path and reliable native first-response, welcome and softkey menu operation. [Handoff](adsi-physical-timing-handoff.md) |
| [ ] Telephony PCP schematics, BOM, connector pinouts and revision-specific routing | Keypad, hook, audio, serial communications and basic supervision work; auxiliary routing and some fault recovery remain inferred. | Verified input and relay routing, warning-tone paths, modem ownership and recovery from peripheral failure. [TP wiring](../../docs/status/TP-PORT-WIRING-SPEC.md) |
| [ ] Control PCP revision matrix, oscillator/crystal frequencies, bus maps, DMA/IRQ wiring and peripheral differences | Earlier and later profiles boot actual firmware; the later clock correction is demonstrated, but not all board/display combinations have independent physical verification. | Accurate clock and resource selection for each supported profile rather than a display-based assumption. [Board profiles](../../docs/board-profiles.md) |

## Priority 2: complete peripheral fidelity

| Reference requested | Acceptance purpose |
| --- | --- |
| [ ] Coin validator and escrow service information: serial protocol, denomination recognition, calibration, reject/jam, collect/refund and mechanical timing | Correct coin routing, faults and recovery beyond INSTALL's successful coin checks. |
| [ ] Cashbox assembly: presence/full/tamper sensors, collector identification and removal/replacement sequence | Accurate vault and cashbox alarms, full-box behavior and collection reports. |
| [ ] Voiceware and audio routing: mixer, attenuation/volume steps, mute logic, speaker/handset paths and playback completion | Match recorded audio levels, routing and prompt completion across hardware profiles. |
| [ ] Flash upgrade and board supervision: write protection, bootstrap selection, power-failure detection, watchdog timing, code-server protocol documentation | Native remote upgrades and interrupted-transfer recovery with only firmware-owned persistent changes. [Download status](../../docs/firmware-download-storage.md) |
| [ ] Inmate terminal service manual, board differences and suitable authorized firmware image | Establish display, card/coin absence, warning-tone and call-control behavior on the actual model. |
| [ ] 11-line VFD module/controller datasheet, brightness circuitry, refresh/ready timing and connector pinout | Validate electrical display behavior and graphics/softkey interaction beyond rendered advertising. [Display handoff](11-line-vfd-black-box-handoff.md) |
| [ ] Portable service display and Desk connector/accessory references | Verify the physical maintenance-only display and desktop-specific wiring independently of successful emulator INSTALL. |

## Useful document and measurement sources

- Hardware design descriptions, complete circuit diagrams and connector maps.
- PCB assembly drawings, BOMs, engineering change notices and manufacturing or service test procedures, with board revision and document issue identified.
- Manufacturer component datasheets, particularly timing and electrical tables.
- For a safely isolated reference terminal, annotated photos, scope captures or audio recordings of the normal peripheral behavior. Record the board revision, firmware version, test conditions, measured units and timebase; do not infer timing from unrelated captures.

## Evidence intake

For each collected reference, record its neutral identifier, revision, relevant page or figure, affected board profile, confirmed electrical behavior, remaining unknowns, and the native test result. Keep locally supplied restricted documents, raw captures, actual payment data and operator details out of public artifacts. Record the observed behavior rather than reproducing restricted material.

Update this checklist as references are reviewed. Track implementation and native acceptance separately in [remaining peripheral qualification](remaining-hardware-qualification.md), the individual handoffs and the [firmware acceptance catalog](../../docs/uat/firmware-acceptance-catalog.md).
