# MAME Z180 / HD64180 support (inspected)

This note is grounded in the vendored MAME tree under `third-party/mame` (pin from `config/mame-pinned.ref` / `build/mame-version.json`).

## Core device headers

| Path | Role |
|------|------|
| `src/devices/cpu/z180/z180.h` | Public CPU API, `Z180()` machine config helper, state enums |
| `src/devices/cpu/z180/z180.cpp` | `z80180_device` implementation |
| `scripts/src/cpu.lua` | `--@src/devices/cpu/z180/z180.h,CPUS["Z180"] = true` enables the device in Genie |

## Declared device types (from `z180.h`)

- `DECLARE_DEVICE_TYPE(Z80180, z80180_device)` — primary Z80180 family device.
- `DECLARE_DEVICE_TYPE(HD64180RP, hd64180rp_device)` — Hitachi HD64180 variant.
- `DECLARE_DEVICE_TYPE(Z8S180, z8s180_device)` / `DECLARE_DEVICE_TYPE(Z80182, z80182_device)` — other Z180-family parts.

## CoinLine driver choice

- Include: `#include "cpu/z180/z180.h"` (see `src/mame/coinline/millennium_state.h`).
- Instantiation (MAME 0.287): `Z80180(config, m_maincpu, 12288000);` in `millennium_state::millennium()` (`millennium_state.cpp`). The helper macro is **`Z80180`**, not `Z180` (there is no `Z180(` shorthand in this tree).
- This binds the `z80180_device` type (`DECLARE_DEVICE_TYPE(Z80180, z80180_device)` in `z180.h`).

## Subtarget Genie flags (`scripts/target/mame/coinline.lua`)

- `CPUS["Z180"] = true` — required for Z180 core and dependencies pulled through `cpu.lua`.
- `CPUS["Z80"] = true` — kept for shared Zilog lineage / tooling expectations in some MAME versions.
- `MACHINES["Z80DAISY"] = true` / `MACHINES["GEN_LATCH"] = true` — minimal machine helpers referenced from the Z180 stack.

## Example pattern in-tree

Other drivers using `Z180(` / `z80180_device` can be found with:

`rg "Z180\\(" src/mame -g'*.cpp'`

Use those as references when extending memory or I/O maps.
