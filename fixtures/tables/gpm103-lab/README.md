# GPM103 lab table pack

Frozen MT22/23/93/134/135 binaries matching the default customer contact
fixture `fixtures/cards/smartcard-sync-gpm103-lab.json`.

| Profile | Load |
|---|---|
| Later / graphical Multi-pay | **MT134** + **MT93** (+ MT135 optional; lab card row uses carrier 255) |
| Earlier Multi-pay | **MT22** + **MT93** (+ MT23 optional) |

Lab identity: PAN `911100`–`911199`, manufacturer `0x28`, customer `0x6F`,
DES key slot `2`, entered lab key `A1B2C3D4E5F60718` (pre-XOR, lab-only).

## CoinLine

1. Create or clone a draft table set for the phone’s firmware profile.
2. Replace/import MT134 (or MT22) and MT93 from these binaries, or paste
   fields from `maximizer-mt93-mt134-gpm103-seed.json` `editor_fields`.
3. Publish, assign, queue download; confirm INSTALL acknowledgements.
4. Boot the emulator with the default lab smart fixture and insert/select
   the customer contact.

These records are lab starters. They are not auto-assigned. Replace the lab
key before any non-lab deployment.

## Emulator default

Portable OEM/graphical launchers select
`smartcard-sync-gpm103-lab.json` unless `[cards] smart_fixture` overrides it.
Tables still come from CoinLine NCC download — the emulator does not inject
these bins into EEPROM by itself.
