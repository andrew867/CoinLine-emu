# CoinLine terminal family handoff

The emulator acceptance library exercises 23 staged firmware IDs through 13
primary isolated CoinLine lab targets, independent V1.1 and V1.3 verification
targets, and a separate MTR 2.0 archived-defaults diagnostic target.
The [acceptance catalog](firmware-acceptance-catalog.md)
lists the wider inventory, exact run evidence, and remaining hardware gates.
The lab provisioning tool records the current assignments and record hashes.

## Host inventory and assignment

The live family catalogue lacks these five entries used by the acceptance
provisioner. The matching lab targets already have explicit firmware versions
and individual published table sets.

| Requested catalogue ID | Intended classification | Evidence to retain |
| --- | --- | --- |
| `mtr113-multipay` | MTR 1.13 Multi-Pay or TTY | Public manager classification 10. The live MT60 report identifies `NNK1F05`, V1.0, operating mode 2. |
| `mtr120-multipay` | MTR 1.20 Multi-Pay | Public manager classifications 20 and 58. Live MT60 reports identify `NPA1S01` and `NPA1F01`, V1.0, operating mode 2. |
| `mtr120-coin-basic` | MTR 1.20 Coin Basic | Public manager classification 18. Terminal report remains unverified. |
| `mtr211-multipay` | MTR 2.11 Multi-Pay | Public inventory classification 51. Live MT60 reports identify `NKA1X02` and `NKA1X05`, V1.1, operating mode 2. |
| `mtr2x-multipay` | Later MTR 2.x Multi-Pay | Live MT60 reports identify `NLA1X05`, `NQA1X01`, `NOK1X03`, and `NOA1X04`, V1.3, operating mode 2. |

Validate the exact firmware build and hardware generation independently of the
terminal family. The 28-byte MT60 report carries the controller ROM identifier,
board version, and operating mode. Its numeric terminal-mode field is 1, 2,
or 3 for card, Multi-Pay, or coin. Map the reported ROM identifier to the
firmware catalogue classification. Keep table layout selection tied to the
registered firmware profile. The public manager uses a universal layout for the MTR 2.0 Multi-Pay
type 36 and an expanded layout for MTR 2.11 Multi-Pay. Assign only the matching
acceptance lab rows when adding family entries and retain their tested table
sets. Read the family and assignment back through the API after updating them.

[Public manager compatibility](https://github.com/hharte/mm_manager#compatibility) ·
[Public manager wire type mapping](https://github.com/hharte/mm_manager/blob/main/src/mm_util.c)

## INSTALL records

| Layout | Record IDs and on-wire lengths, including the type byte | Acceptance |
| --- | --- | --- |
| MSR 1.7 legacy | MT21 18, MT24 721, MT74 819 | Full Multi-Pay, Card-only, and Desk REL 2 INSTALL and restart. |
| MTR 1.9 universal | MT21 18, MT22 661, MT23 678, MT92 3401, MT101 403 | Full Multi-Pay INSTALL and restart. Both Desk REL 1 images, `NBJ1S03` and `NBJ1F03`, acknowledged ten records and returned to service after restart. |
| MTR 1.20 expanded | MT21 48, MT92 3061, MT134 1153, MT135 1109, MT136 203 | Both a Spanish and a French Multi-Pay image accepted all 11 records and returned to service after restart. |
| MTR 2.11 V1.1 expanded | Expanded layout above plus MT72 1001, MT85 8001, MT86 8001 | Both V1.1 program images mark MT85 and MT86 mandatory. Their provisional 12-record sets ended at status 34. Both accepted all 14 records in the revised published set and completed INSTALL and warm restart after the Z180 interrupt sampling correction. Their chip-card checks failed. |
| Later V1.3 expanded | Expanded layout above plus MT72 1001 | All four V1.3 images completed the 12-record download, INSTALL and in-service restart after correcting the native EEPROM decoder. They use their default visual prompts. Their optional chip-card tests failed. |
| MTR 2.0 universal | MT21 18, MT22 661, MT23 678, MT92 3401, MT101 403 | `NCA1X03` ACKed all eleven records in two complete consecutive NCC calls and still reported status 34. A read-only physical bus watch captured all ten mandatory table flags as `0x80` immediately before the failure display. A separate 24-record trial included the public compatible defaults, received every MT14 and the host's MT13, then reported status 23. The original universal set remains assigned to the primary target. |

For MT26, the model byte is 1 for Desk/Card, 2 for Multi-Pay, and 3 for Coin
Basic. Desk/Card omit coin validation. Coin Basic omits the card table.
Retain the per-terminal wire ID in MT21 and verify the NCC dial fields for
each enabled access record. Link acknowledgement, MT14 record acceptance,
completion of the post-download dialogue, and in-service restart are separate
checks.

## Cash-box status requested during INSTALL

Captured MT44 requests from `NCA1X03`, `NKA1X02`, `NKA1X05`, and `NLA1X05`
have reason `0x86` (INSTALL, lost memory, and cash-box status requested).
The `NCA1X03` retry sends `0x82` (INSTALL and cash-box status requested).
The host replies with a 12-byte NCC frame containing only MT18. The public
manager sends a 57-byte MT38 cash-box status record immediately after MT18
when the request's `0x80` bit is set. Restore the terminal-specific persisted
cash-box status, or the normal empty state for a new terminal. The record
has a six-byte timestamp, status, percent full, little-endian currency value,
and eight little-endian coin counts, with reserved fields. Preserve MT18-only
behavior when the bit is clear. Check this exchange using a synthetic empty
terminal before repeating the V1.0/V1.1 INSTALL. The missing status reply is
an observed protocol gap. Its relationship to INSTALL status 34 remains unverified.

A diagnostic loopback relay appended the public manager's 57-byte empty-box
record to the host's MT18 response for `NCA1X03` and `NKA1X02`. Both received
the table set and requested another download with reason `0x02`, with the
cash-box request bit cleared. Status 34 remained. The MT38 reply resolved the
cash-box request while another required-update condition persisted.
Direct host delivery still needs the documented MT38 behavior.

[Public manager MT44 handler](https://github.com/hharte/mm_manager/blob/main/src/mm_manager.c) ·
[Public MT38 record layout](https://github.com/hharte/mm_manager/blob/main/src/mm_manager.h)

An earlier `NPA1F01` run accepted all 11 expanded records and timed out after
its last MT14. A fresh run completed the dialogue, INSTALL, and restart using
the same assignment. If it recurs, correlate MT136 acknowledgement, MT13
delivery, subsequent terminal dialogue, and close. Direct trials of `NBJ1S03`
and `NBJ1F03` ACKed MT20, then sent NAK plus clear before MT44. `NBJ1S03`
later accepted the assigned record set through a transparent capture relay
and over a direct connection. All seven staged split-flash candidates entered
INSTALL and reached the later board's CO line check. The later board's modem
serial readback and DTMF routing corrections also enabled the first expanded
firmware download. Retain the additional MT72 record for the affected later
profiles. Its default payload comes from the public manager's table 0x48.
