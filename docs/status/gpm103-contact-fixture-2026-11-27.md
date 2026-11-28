# GPM103-family contact fixture qualification

The bundled `smartcard-sync-gpm103-contact.json` is a bounded 16-byte
synchronous EEPROM bench pattern, independent of any stored-value
application. Its last byte contains two writable test bits. The 10 ms
nominal programming delay and fixture capacity are test settings, not
measured original-chip limits.

| Qualification | Result |
| --- | --- |
| Earlier-board native contact reads and timed programming | Pass |
| Later-board native contact reads and timed programming | Pass |
| Short pulse, read-only bounds and removal during a pending write | Pass on both boards |
| Reinsertion and persistence across emulator restart | Pass on both boards |
| Installed graphical firmware, Craft 728 | Smart Card Test Okay; 8 bits read, 0 mismatches; returned to service |
| Installed two-line MTR 2.x firmware, Craft 728 | Smart Card Test Okay; 8 bits read, 0 mismatches; returned to service |

Native pin probe evidence: `build/gpm103-contact-fixture-native/20261127T232729.153453Z/`.
Installed-terminal maintenance evidence:
`build/gpm103-contact-craft/20261127T232929.129896Z/` and
`build/gpm103-contact-craft/20261127T233031.641357Z/`.

The native device model supplies the contact responses; no terminal EEPROM
or SRAM was modified to obtain the result. Both installed-terminal tests
used isolated copies of previously installed storage and disconnected NCC
settings. No application acceptance or call transaction is asserted by
these results.
