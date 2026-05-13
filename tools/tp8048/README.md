# Telephony Co-Processor (TP 8048) Firmware Builder

This directory contains the original behavioural firmware for the telephony
co-processor used by the CoinLine MAME machine, plus a thin Python wrapper that
assembles it into a 4 KiB ROM image.

The target chip family is the **PCD3349A**, an MCS-48 / 8048-class
microcontroller commonly used for terminal-side telephony logic in this class of
hardware. The assembly here is original code that runs under any MCS-48-compatible
emulator core; it is *not* a dump of any vendor's mask ROM.

## Layout

```
tools/tp8048/
├── build_tp_rom.py         # Cross-assembler driver + deterministic fallback
├── src/
│   ├── telephony_subprocessor.asm        # Behavioural TP firmware (MCS-48)
│   └── telephony_subprocessor_symbols.inc  # Opcodes, ports, state constants
└── bin/                    # Optional: drop asl/p2bin here to assemble offline
```

The build emits `firmware/telephony_subprocessor.rom` (4 KiB) at the repo root.

## Building

From the repo root:

```bash
python tools/tp8048/build_tp_rom.py --allow-placeholder
```

The script tries real assemblers in this order:

1. `asl` + `p2bin` (Macro Assembler AS) — bundled under `tools/tp8048/bin/` if
   present, otherwise picked up from `PATH`.
2. `as8048`
3. `sdas48` + `sdobjcopy`

If none are installed, `--allow-placeholder` writes the same deterministic
fallback pattern that the C++ TP backend recognises, so acceptance/CI runs
remain reproducible even on a fresh checkout without an MCS-48 toolchain.

## What the firmware does

The TP firmware sits between the main CP (Z180) and the front-panel/handset
side of the board. It:

- Mirrors keypad and hookswitch state into CP-visible opcodes.
- Emits boot-ack and steady-state hook bytes with realistic timing.
- Drives DTMF / dial-tone / NIS-tone intent via HGF/LGF register writes.
- Handles CP configuration and query commands over the CSI/O byte stream.

See [`docs/tp8048/execution-spec.md`](../../docs/tp8048/execution-spec.md) for
the full execution and clocking spec, and
[`docs/tp8048/asm-state-machine.md`](../../docs/tp8048/asm-state-machine.md)
for the state-machine notes that match the assembly source.

## Quick reference: boot-ack opcode

`QUEUE_BOOT_ACK` mirrors the documented CP↔TP boot opcode contract:

- **P1.7 high (on-hook)** → emits CP `POWER_ON_RESET` (**0x70**).
- **P1.7 low (off-hook)**  → emits CP `POWER_ON_ACK`   (**0x72**).
