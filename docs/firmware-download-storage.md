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
The alias is disabled by default. The format and timing of the actual
download protocol still need qualification with a compatible code server.

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
sequence, wire transfer and recovery behavior still require live acceptance.

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
program. Selection 7 also enables the upper SRAM window. The SRAM windows
retain their own bank selection. The model wraps the two bank-address bits
in that upper window, an inference consistent with the 512 KiB SRAM capacity
and the observed bootstrap behavior.
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

1. Cold boot through the flash bootstrap and complete normal INSTALL.
2. Record the initial flash, SRAM/EEPROM and firmware version evidence.
3. Deliver MT94 through the NCC on an isolated local terminal.
4. Capture the terminal's independent code-server call and download protocol.
5. Transfer an approved compatible image using its documented download format.
6. Observe firmware-issued flash commands, checksum verification and a firmware-initiated reboot.
7. Confirm the active flash hashes, reported firmware version and service behavior across another power cycle.
8. Repeat interrupted-transfer cases and check the resident recovery application and retained state.

The host integration is still required for the end-to-end tests. The
historical extended staging fixture lies outside the processor's 20-bit
physical address space. It is retained for existing synthetic storage tests.

### Native schedule pickup

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
assertion; a subsequent separate NCC contact did reach carrier. The current
modem's shared five-second call-progress carrier interval is longer than
the observed code-server TCP connection lifetime. This does not establish
the expected physical modem timing or application handshake, which need
separate observation rather than an assumed successful connection.

The session was ended normally after the first observation. The persisted
flash file remained byte-identical to the pre-schedule reference. Native
schedule pickup and separate dialing/routing are qualified; carrier,
application handshake, image delivery and actual flash programming are not.

The observation schedule was safely disabled afterward. A subsequent normal
warm contact acknowledged a second MT19 callback, returned its own MT9 and
acknowledged the manager's disabled MT94. The observation endpoint was
closed, and the terminal returned to ordinary operation. Evidence:
`build/firmware-acceptance/nla1x08-code-disabled-callback/20261126T052319.695480Z/`.

## Tests

`test_flash_model` exercises NOR command sequencing, program and erase
timing, electronic identification, zero-to-one programming restrictions, and
the later-board low-window address remap.

`test_mach_pio_c0` checks the separate SRAM bank selection and wrap.
`test_modem_code_server` checks the independent loopback data-call route,
carrier establishment, unchanged binary data in both directions, relay
release and ordinary NCC route selection.
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
the synthetic image. Actual code-server delivery and OEM application
upgrade validation remain separate acceptance gates.

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
also passed. This is preparation for a genuine NCC-issued MT94 job and
compatible code-server session, not an end-to-end remote upgrade result.

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
does not claim to reproduce. The original firmware's recovery logic after
an interrupted code-server transfer has not yet been exercised.

The interrupted, cold and warm native probes all passed on the subsequent
coin/cashbox build as well, with no saved seed changes during the interrupted
phase. Evidence: `build/firmware-acceptance/flash-bus-probe/20261126T024018.964188Z/`.
Live qualification still requires an isolated NCC assignment and code-server
listener, an approved compatible target image, and observation of the
firmware-initiated transfer, resulting flash state and actual reboot.
