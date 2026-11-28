# GPM103 lab image as default customer contact

The portable launcher default sync contact is
`fixtures/cards/smartcard-sync-gpm103-lab.json`. Both example INI files leave
`smart_fixture` blank so firmware-specific asynchronous defaults are preserved.
The automatic alternate for an asynchronous primary is the generic read-only
synchronous transport fixture. No writable fixture is automatically assigned
to a secondary contact slot.

The `--gpm103-lab` probe selects the independent frozen read-only fixture
`fixtures/cards/smartcard-sync-gpm103-lab-readback.json` instead of the
writable default. The original contact and short-memory benches remain
unchanged. The existing table directory is staged as files only and checked
for file presence and manifest integrity. No OEM application, writable
default, or commercial interoperability qualification is claimed.

## Regression validation

The portable profile suite passed all 24 tests, including both copied example
INIs, read-only automatic secondary slots, readback probe preflight, all
eight table-directory staging entries, and the five binary file digests and
lengths.

The independent lab readback, original contact bench, and short-memory
bench passed on both native board mappings using the existing executable
(SHA-256 `b65f84060cbca2d1a7efa090770fbb6336709c30a38112388706334d11ef276c`).
Both original benches also passed their existing restart persistence checks.
The native runs used explicit fixture paths, not a new build or default
customer-contact application test.

Local run evidence:

- `build/firmware-acceptance/pr38-gpm103-lab-readback/20261128T021412.098864Z/`
- `build/firmware-acceptance/pr38-contact-bench/20261128T021416.349794Z/`
- `build/firmware-acceptance/pr38-13byte-bench/20261128T021424.522859Z/`
