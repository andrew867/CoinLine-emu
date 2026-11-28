# GPM103 lab fixture readback

The frozen 13-byte synchronous lab image is available as
`fixtures/cards/smartcard-sync-gpm103-lab.json`. It is read-only and separate
from both existing contact benches. The `--gpm103-lab` probe passed exact
13-byte contact readback on the earlier and later board mappings.

The unchanged `--gpm103-contact` and `--13-byte-bench` cases passed on both
board mappings, including their existing restart persistence checks. All 17
portable profile and fixture tests passed. A Windows portable ZIP was built
using the existing native executable; the lab fixture in the archive matched
the repository file byte for byte and its SHA-256 manifest entry matched.

Native executable SHA-256:
`b65f84060cbca2d1a7efa090770fbb6336709c30a38112388706334d11ef276c`.

Local evidence:

- `build/firmware-acceptance/gpm103-lab-readback/20261128T012512.337630Z/`
- `build/firmware-acceptance/gpm103-contact-regression/20261128T012516.305720Z/`
- `build/firmware-acceptance/13byte-bench-regression/20261128T012525.524275Z/`
- `build/dist/gpm103-lab-readback/CoinLine-Emulator-OEM-Windows-x64-20261128T012613.244444Z.zip`

These native runs qualify lab fixture readback only on the existing executable.
No new application-path, programming, commercial interoperability, or OEM
table acceptance qualification is claimed.
