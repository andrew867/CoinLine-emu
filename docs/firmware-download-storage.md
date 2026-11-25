# Remote firmware download on MTR 2.x

## Terminal download sequence

The Code Download table (MT94) provides the target revision, code-server
access number, timing and retry parameters, and a block-selection bitmap.
The terminal initiates its own call to the code server when the download
conditions are satisfied. The Manager database design report, Issue 00.02,
pp. 2-119 to 2-122, describes these fields.

The download application processes validated 4 KiB blocks. The application
retains checksum tables and progress information, backs up unaffected data
from an erase sector to SRAM, erases the selected NOR sector, and programs
and verifies the received blocks and unaffected data. It can resume an
interrupted transfer using the retained information. The full upgrade
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

[ST M29F040B data sheet, tables 5–7](https://media.digikey.com/pdf/Data%20Sheets/ST%20Microelectronics%20PDFS/M29F040B.pdf)

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

## Tests

`test_flash_model` exercises NOR command sequencing, program and erase
timing, electronic identification, zero-to-one programming restrictions, and
the later-board low-window address remap.

`test_mach_pio_c0` checks the separate SRAM bank selection and wrap.
The native synthetic flash-bus probe writes one byte to each chip with
normal Z180 instructions executing from SRAM. On the second launch, the
program reads both saved values without issuing program commands again.
The probe additionally checks the persistent image header and bytes and
the unchanged seed image.

```sh
python tools/acceptance/flash_probe.py --mame <path-to-mamecoinline>
```

Both phases passed in
`build/firmware-acceptance/flash-bus-probe/20261125T004352.488404Z/`.
This establishes native CPU-bus programming and restart persistence for
the synthetic image. Actual code-server delivery and OEM application
upgrade validation remain separate acceptance gates.
