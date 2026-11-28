# GPM103 lab image as default customer contact

The portable launcher default sync contact is
`fixtures/cards/smartcard-sync-gpm103-lab.json` (`memory_hex`
`00286F00000001B978FFFFFFFF`, counter programming bits 64–104). Example INI
files set `smart_fixture` to that path. Matching MT binaries are packaged under
`fixtures/tables/gpm103-lab/` for CoinLine draft import / Maximizer paste.
The `--gpm103-lab` probe still asserts virgin readback only
(`COINLINE_SYNC_READBACK_ONLY`); it accepts the writable programming window
as long as the frozen hex and bit range match.

Transport benches (`smartcard-sync-gpm103-contact.json`,
`smartcard-sync-13byte-bench.json`, `smartcard-sync-test.json`) remain packaged
and selectable. OEM application acceptance still requires INSTALL of the lab
MT93+MT134 (or MT22+MT93) records; the emulator does not write those tables
into EEPROM without CoinLine.
