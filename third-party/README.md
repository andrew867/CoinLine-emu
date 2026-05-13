<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# External dependencies (third-party)

## MAME source tree

MAME is GPL-2.0-or-later and is **not** cloned into `third-party/mame/` by default (that directory is gitignored for local checkouts).

Clone MAME separately and register the driver:

```bash
export EXTERNAL_MAME_ROOT=/absolute/path/to/mame
./scripts/register-millennium-driver.sh
```

Add the printed `src/mame/coinline/*.cpp` lines to `src/mame/mame.lst`, run `make REGENIE=1`, and build.

Set `COINLINE_EMU_ROOT` to the absolute path of this repository when running MAME so `fixtures/` paths resolve. Use `COINLINE_FIRMWARE` for the operator `../firmware/flash.bin` path until native `-firmware` / `-board` options are wired in MAME core (see `specs/mame-machine-driver.spec.md`).
