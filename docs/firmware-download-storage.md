# Remote firmware download on MTR 2.x

## Terminal download sequence

The Code Download table (MT94) provides the target revision, code-server
access number, timing and retry parameters, and a block-selection bitmap.
The terminal initiates its own call to the code server when the download
conditions are satisfied. The Manager database design report, Issue 00.02,
pp. 2-119 to 2-122, describes these fields.

The local modem supports a separate dial alias for this call. Configure
the following in the operator's `coinline.ini` before assigning MT94
in CoinLine. The number and port shown are example lab settings.

```ini
[line]
ncc_port = 4000
code_server_number = 2130706434
code_server_port = 4001
```

The code-server alias must be a distinct ten-digit string, and the server
must listen on the configured loopback TCP port. The ordinary NCC number
still selects the NCC port. The same firmware-controlled modem, carrier,
and ASCI1 path transports raw code-server bytes without adding an NCC frame.
The alias is disabled by default. Native qualification with the stock
download application completed the separate call, modem training and
documented block-transfer protocol with a compatible loopback code server.

The batch acceptance runner and interactive later-board `graphic_session.py`
accept the same optional `code_server_number` and `code_server_port` in
the local settings JSON. They validate the alias before launching and keep
ordinary NCC routing unchanged. Use a copy of an independently installed
lab terminal's battery and flash state, not the operator's running phone.

The download application processes validated 4 KiB blocks. The application
retains checksum tables and progress information, backs up unaffected data
from an erase sector to SRAM, erases the selected NOR sector, and programs
and verifies the received blocks and unaffected data. The retained information
is intended to support interrupted-transfer recovery. The full upgrade
sequence, wire transfer and reason-2 retained retry are qualified below for
the registered two-line lab target.

## Later control-board flash

The archived split-chip firmware inputs contain two 512 KiB flash images.
On boards fitted with ST M29F040B devices, each chip has eight 64 KiB erase
blocks and independent byte-program and erase command state. The emulated
device provides electronic identification, byte programming, block and chip
erase, status polling and unlock-bypass programming. The typical program
and erase timing follows the device data sheet. Erase suspend and physical
block protection remain unqualified.

The later board's low 64 KiB address window selects flash sectors C through F
for latch selections 0 through 3, with the resident bootstrap at C and the
download-application banks at D and E. Selections 6 and 7 select the main
program. Selection 7 also enables the upper SRAM window. Common SRAM
remains fixed while the lower ROM window changes. The upper SRAM window
exposes the next linear physical area; further addressing of the remaining
capacity of the 512 KiB device is unqualified. Native loader traces exposed
the earlier incorrect high-latch-bit bank assumption: after the F9 to 07
remap, the same transfer-instruction addresses returned erased bytes.
The decode correction restores continuity without preloading any RAM.
The emulator exposes the bootstrap and writable flash through normal
memory-bus transactions. The initial images are validated by SHA-256 and
are retained as read-only input files.

The emulated flash contents persist separately from battery SRAM and the
serial EEPROM. The saved flash image is tied to the initial image digest,
and a separate launch restores the same chip contents. An incomplete
operation at emulator power-off preserves the last completed writes.
Physical partial-program or partial-erase corruption is not modeled.

Normal INSTALL and a separate in-service restart passed on both V1.1
MTR 2.11 references and the V1.3 MTR 2.12 reference through the restored
bootstrap. Each restored its firmware-written battery state and persisted
flash, with no change to its active flash digest during ordinary provisioning.

[ST M29F040B data sheet, tables 5 to 7](https://media.digikey.com/pdf/Data%20Sheets/ST%20Microelectronics%20PDFS/M29F040B.pdf)

## Acceptance gates

1. **Passed.** Cold boot through the flash bootstrap completed normal INSTALL.
2. **Passed.** Cold and warm baseline flash, battery and version evidence was recorded.
3. **Open.** A fixture delivered MT94 and received MT14; production CoinLine scheduling and delivery is not yet qualified.
4. **Open.** The terminal dialed a distinct loopback fixture and completed the modem handshake; the production hosted listener is not yet qualified.
5. **Open.** Only a generated marker target was transferred. A distinct registered firmware target has not completed the production path.
6. **Open.** Persistent bytes changed in the fixture run, but guest-issued NOR operation evidence was not captured for a registered target.
7. **Open.** The fixture run retained the baseline reported version, and its evidence recorded service and restart as unqualified.
8. **Open.** The packet-2 retry observation used the fixture server and must be repeated through production CoinLine.

Detailed evidence is in
[MTR 2.x firmware download qualification](status/firmware-download-qualification-2026-11-28.md).
The historical extended staging fixture lies outside the processor's 20-bit
physical address space. It is retained for existing synthetic storage tests.

### Retracted fixture-only observation

The isolated two-line standard universal profile started from active flash
SHA-256 `b09f9c64817f52522cdb4a01f43cdfe5422eb65cd087defeec2906e597d60e34`
and program tag `1.3`. CoinLine assigned block 128 by MT94 on the ordinary
NCC connection, and the terminal acknowledged that record with MT14. At the
scheduled time it dialed the distinct code-server alias and completed the
guest-controlled answer, mark, S1 and QAM states before carrier.

The code server delivered four 1 KiB packets and one padded 128-byte packet,
followed by EOT. The terminal ACKed all five packets and EOT, displayed its
checksum and block-write progress, updated block 128 plus checksum-metadata
block 191, and restarted the main application. The active flash SHA-256 was
`38c8b8d802920a5e43fa52f3739bd0d9d85127e7b30b74915ee685ee1e77b142`,
an exact match for a computed expected active image. A separate warm boot
reported `DLA 1 active ver 7` and `X1.3`, preserved that digest, delivered the
physical receiver-lift event to the control processor, and returned on-hook.

This did not establish a real firmware update. The target was generated from
the baseline by adding a marker to one erased block, the reported version did
not change, and the host processes were fixture tools rather than production
CoinLine services. The preserved report recorded service and restart as
unqualified. The acceptance gates above therefore remain open.

For the interruption case, the server disconnected after transmitting packet
2. The terminal ACKed packet 1, emitted CAN on connection loss, left active
flash byte-identical to the baseline, and retained a redial callback. After a
power cycle it retried automatically without another MT94. The request reason
changed from 1 to 2, block 128 was retained, the transfer and apply completed,
and the resulting active image again matched the registered target exactly.

### Earlier native schedule pickup observation

An independently installed graphical MTR 2.12-based reference was restarted
from its own firmware-written battery and persistent flash. Normal Craft
item 369 initiated a full MT44 refresh and completed all 18 assigned tables;
it did not deliver the separate pending code-download job. The persisted
flash file had the same SHA-256 before and after the refresh. The isolated
manager then prepared a distinct code-only schedule and special callback.
On another normal warm restart, the terminal acknowledged MT19, initiated
its own MT9 callback return, received a single 100-byte MT94 record and
acknowledged it with MT14. The host closed the ordinary NCC call normally,
without delivering unrelated tables or any code-image payload.
Evidence: `build/firmware-acceptance/nla1x08-code-observation-ready/20261126T045324.769020Z/`
and `build/firmware-acceptance/nla1x08-code-callback/20261126T050301.898610Z/`.
Schedule pickup is thereby qualified for this reference. At the scheduled
time, the terminal seized its modem relay, outpulsed the distinct configured
code-server alias, and the native modem opened the separate TCP connection.
The observation endpoint accepted that connection and received and sent zero
bytes. The native trace records relay release before an explicit SSI carrier
assertion; a subsequent separate NCC contact did reach carrier. That earlier
build used a shared five-second call-progress interval longer than the
observed code-server TCP connection lifetime. This observation established
the schedule and route only. The later two-line qualification above supersedes
its carrier and application-handshake gap.

The session was ended normally after the first observation. The persisted
flash file remained byte-identical to the pre-schedule reference. At that
checkpoint, schedule pickup and separate dialing/routing were qualified,
while carrier, application handshake, image delivery and actual flash
programming were not yet exercised.

The observation schedule was safely disabled afterward. A subsequent normal
warm contact acknowledged a second MT19 callback, returned its own MT9 and
acknowledged the manager's disabled MT94. The observation endpoint was
closed, and the terminal returned to ordinary operation. Evidence:
`build/firmware-acceptance/nla1x08-code-disabled-callback/20261126T052319.695480Z/`.

### Earlier graphical carrier observation

The independently cold/warm-qualified full graphical image (SHA-256
prefix `996a8ac5e81d`) accepted a separate scheduled code-only callback.
The manager persisted a genuine MT19 acknowledgement, MT9 callback return
and one MT94 application ACK, without unrelated table delivery. The
terminal's independently verified calendar was correct before scheduling.
The receive-only code endpoint was isolated from ordinary NCC and supplied
no code payload.

At the scheduled checkpoint the terminal opened the distinct code-server
TCP connection at emulated 267.337067 s. Its native SSI reads observed
ringback, answer tone at 269.346267 s, and the existing mark-phase value
`28` at 270.846267 s. At 271.035174 s the actual terminal released its
modem relay, before the programmed carrier phase. A bounded read-only
observation independently captured the resident loader's carrier-wait
state and its failure branch when all four qualified line indications
were absent. No application handshake or image bytes were exchanged.
The persistent flash file was byte-identical before and after this run;
both terminal memory-check markers remained intact.

The [73K324L manufacturer datasheet, pp. 10-11 and 20](https://static6.arrow.com/aropdfconversion/aa1da1e214c115791f9da7f15337dacb4abdfc16/fds73k324l_v7.0.pdf)
distinguishes the carrier indication (D3), unscrambled-mark indication
(D4), received data (D5), and S1 pattern (D6). The current `28` value
contains carrier and receive-data bits, not an independent unscrambled-mark
indication. The [ITU-T V.22bis recommendation, section 6.3 and Figure 5](https://www.itu.int/rec/T-REC-V.22bis-198811-I/en)
defines the distinct training sequence beginning at 1200 bit/s for a
2400 bit/s connection. The native terminal's retained detector settings
specify a 50 ms nominal answer release and 100 ms nominal carrier
qualification. The observed all-zero interval is consistent with a gap
between these independent qualifications. This observation motivated the
separate detector states and guest-controlled training now covered by the
two-line native qualification. Neither a permanently asserted answer tone
nor a forced data-carrier status was used for the passing transfer.

Evidence:
`build/firmware-acceptance/nla1x09-code-phase-observation/20261126T082451.792563Z/`.

The separate code endpoint now has a register-level answering modem model
with the [V.25 answer sequence and turn-off gap](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-V.25-198811-S%21%21PDF-E&lang=e&type=items),
distinct carrier and unscrambled-mark indications, an S1 response to a
complete guest-transmitted S1 pattern, and subsequent scrambled-mark and
QAM training phases. A V.22 fallback is modeled separately. Nominal
detector acquisition/hold intervals are selected within the manufacturer's
specified bounds; the ordinary NCC peer's existing timing remains
unchanged. Carrier completion requires the guest's actual modem-register
programming, not solely TCP connection or elapsed time. This is a
logical pin-level abstraction, not an analogue waveform modem. The two-line
native run above completed the stock application transfer through this model.

The component regression verifies the initial no-signal gap, separate
detect bits, no data transfer before training, genuine SSI-programmed S1
and QAM transition, 1200 bit/s fallback, and a complete 4 KiB binary bridge.
The native run verifies resident-loader interoperation separately.

## Tests

`test_flash_model` exercises NOR command sequencing, program and erase
timing, electronic identification, zero-to-one programming restrictions, and
the later-board low-window address remap.

`test_mach_pio_c0` checks common SRAM continuity across the ROM selector,
upper-window selection and the observed linear address bounds.
`test_modem_code_server` checks the independent loopback data-call route,
carrier establishment, unchanged binary data in both directions, relay
release and ordinary NCC route selection. It reads the code peer's
ringback, answer-tone gap, raw carrier, separate mark, S1 and training
states through normal SSI pins, and checks both V.22bis and V.22 fallback.
The opt-in native modem trace records corresponding call-progress
transitions, relay/TCP events, changed SSI detect reads and register writes
without synthesizing a protocol response. The native application trace records
the successful response bytes separately.
`test_firmware_download_staging` and `test_firmware_download_apply` exercise
the deterministic staging and apply models and are registered under the
`firmware_download` CTest label.
The native synthetic flash-bus probe starts with non-erased bytes in one
sector of each chip, erases those sectors, polls the device ready state and
programs the target bytes with normal Z180 instructions executing from SRAM.
It checks that other bytes in the erased sectors became `0xff` and adjacent
sectors retained their data. On the second launch, the program reads both
saved values without issuing erase or program commands again. The probe
additionally checks the persistent image header and the unchanged seed image.

```sh
python tools/acceptance/flash_probe.py --mame <path-to-mamecoinline>
```

Both phases passed in
`build/firmware-acceptance/flash-bus-probe/20261125T004352.488404Z/`.
The final build with the independent code-server route also passed both phases
in `build/firmware-acceptance/flash-bus-probe/20261125T010329.378553Z/`.
The expanded native erase-and-program probe passed both phases in
`build/firmware-acceptance/flash-bus-probe/20261125T010939.750945Z/`.
This establishes native CPU-bus erase, programming and restart persistence for
the synthetic probe. The stock application and code-server acceptance remained
separate at that checkpoint and are now covered by the two-line run above.

## Read-only pre/post upgrade evidence

When the emulator is closed, compare its persisted `program_flash` against
the original mapped seed and, when available, the pre-upgrade saved state.
The comparison checks the saved image's seed binding and 1 MiB geometry,
records both chip hashes and all changed 4 KiB block indices, and optionally
compares requested blocks against an independently approved target image.
It makes no writes to firmware, flash or battery and never asserts that a
matching flash snapshot establishes a successful reboot or service state.

```sh
python tools/acceptance/firmware_upgrade_evidence.py \
  --seed <original-mapped-image> --before <pre-run-program_flash> \
  --after <completed-run-program_flash> --expected-image <approved-target-image> \
  --output <new-report.json>
```

For a selected-block job, repeat `--expected-block <index>` for the actual
job's zero-based 4 KiB block indices. Changes outside the requested set are
reported separately rather than automatically accepting them; firmware
may write its own metadata and each unexpected change needs explanation.
Without an independently supplied target, the tool reports changes only.

The latest native synthetic dual-flash probe passed cold and warm phases,
and the evidence tool independently identified changed blocks 1 and 130
in `build/firmware-acceptance/flash-bus-probe/20261125T223420.771891Z/`.
The separate code-server modem route, NOR model and bank mapping tests
also passed. Those probes were preparation for the later NCC-issued MT94 and
compatible code-server result; they are not used as substitutes for it.

The optional `flash_probe.py --interrupt-before-complete` bench closes
the synthetic emulator while the actual erase bus returns a busy-toggle
status. It checks that the saved image still matches the seed byte for
byte, restarts from that saved state, completes the two-chip erase and
program operations, then verifies persistence on another launch.
All three phases passed in
`build/firmware-acceptance/flash-bus-probe/20261125T223851.710800Z/`.
This tests the existing deterministic completed-operation persistence
model and an orderly emulator shutdown during a busy operation. A real
power failure can produce partial physical corruption, which this model
does not claim to reproduce. The stock application's separate recovery logic
is now exercised by the packet-2 disconnect and retained retry above.

The interrupted, cold and warm native probes all passed on the subsequent
coin/cashbox build as well, with no saved seed changes during the interrupted
phase. Evidence: `build/firmware-acceptance/flash-bus-probe/20261126T024018.964188Z/`.
The later two-line qualification supplied the isolated NCC assignment,
code-server listener, registered compatible target and native apply evidence
that these earlier probes did not provide.

The next full graphical retry remains held at normal schedule cancellation.
A native Craft refresh completed all 18 assigned records and the terminal
then returned its pending special callback on the same NCC connection. The
manager acknowledged the callback but did not deliver the separately queued
disabled MT94 because its ordinary install-dialogue guard was still active.
The manager-side focused regression now passes; deployment into the isolated
test environment is pending. The terminal is offline, with its retained
state preserved. No replacement schedule, modem-table change or image
transfer has occurred on that graphical reference. Its status is independent
of the completed two-line standard universal qualification.
