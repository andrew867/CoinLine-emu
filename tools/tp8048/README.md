# TP 8048 ROM Builder

Builds `firmware/tp_pcd3349a_behavioural.rom` from:

- `rom/tp_pcd3349a_behavioural.asm` and its companion include

## Usage

From repo root:

`python tools/tp8048/build_tp_rom.py`

To build it on Ubuntu using the pinned Macroassembler AS toolchain from CI,
run `bash tools/tp8048/build-ci-rom.sh build/asl` from the repository root.

Without `--allow-placeholder`, the script requires one of:

- Bundled **`third-party/aswcurr/bin/asl.exe`** + **`p2bin.exe`** (preferred on this repo)
- `asl` + `p2bin` on `PATH`
- `as8048`
- `sdas48` + `sdobjcopy`

Assembly runs with the source directory as the working folder so `INCLUDE` resolves (same requirement if invoking ASL manually).

## Output

- Default ROM path: `firmware/tp_pcd3349a_behavioural.rom`
- Size: 4096 bytes (PCD3349A internal ROM size)

## Note

Use the assembled image for firmware acceptance and distributable packages. The optional `--allow-placeholder` flag is for tooling experiments.

Behavioural `QUEUE_BOOT_ACK`: **P1.7 high (on-hook)** emits CP `POWER_ON_RESET` (**0x70**); **P1.7 low (off-hook)** emits `POWER_ON_ACK` (**0x72**), matching the documented CP↔TP boot opcode contract.
