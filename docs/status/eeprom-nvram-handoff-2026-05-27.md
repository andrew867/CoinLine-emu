# EEPROM and battery persistence

Updated 2026-11-24. This status supersedes the initial EEPROM bring-up snapshot.

## Hardware interface

The EEPROM is accessed through Microwire clock, data and chip-select signals.
The earlier board uses two opcode bits and a six-bit word address. The later
board uses two opcode bits and an eight-bit word address. Both shift sixteen
data bits for a WRITE and read the dummy zero followed by sixteen data bits.
Write enable, write disable and erase are decoded from the command fields.

A cold later-board trace in
`build/firmware-acceptance/runs/20261124T214040.081737Z/` records 29 rising
clock edges per READ: an idle clock, START, ten opcode/address clocks, and
seventeen read clocks. The previous later-board decoder waited for eleven
clocks after START, consuming the first WRITE data bit. The corrected decoder
uses the physical command length on both board revisions.

## Persistence and acceptance

Each cold acceptance run begins with erased EEPROM and cleared SRAM. The
terminal's actual writes update the EEPROM model and the backing battery file.
The separate warm launch uses only the battery produced by that cold run.
No terminal install flag or downloaded table is supplied by the runner.

Both MTR 1.9 Multi-Pay program candidates completed cold INSTALL and warm
restart before the decoder correction. The earlier command geometry is retained
and covered by the Microwire regression test.

MTR 2.12 `NLA1X05` and `NOA1X04` completed coin calibration and escrow testing,
then reported INSTALL status 71. The latter run's saved EEPROM remained entirely
erased. The later-board decoder correction is covered by a test that sends
actual command clock sequences, checks write-enable protection, and reads
back distinct low and high addresses. After the fix, `NLA1X05`, `NQA1X01`,
`NOK1X03`, and `NOA1X04` each completed INSTALL and returned to service on a
separate launch. All four battery EEPROM arrays had 156 bytes changed from erased state. The earlier
MTR 1.9 regression also passed both program candidates.

## Reproduction

From the repository root in the supported MSYS2 MINGW64 environment:

```sh
python tools/acceptance/run.py --only NQA1X01 --eeprom-trace --io-trace
```

The runner saves a per-attempt Microwire transaction trace, NCC frame report,
display events, battery image and SHA-256 evidence under
`build/firmware-acceptance/runs/`. A completed download alone does not certify
EEPROM persistence. The cold INSTALL must reach its service prompt and the
separate warm launch must return to service using the saved battery.

See the [firmware acceptance catalog](../uat/firmware-acceptance-catalog.md)
for the current firmware-specific outcomes.
