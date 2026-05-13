# Running CoinLine Terminal Emulator

This document describes how to launch `coinline-emu` once builds are available. Until tranche E10 (boot-to-idle) is reached, runs may not progress beyond early boot milestones; see [`docs/boot-milestones.md`](docs/boot-milestones.md) for the milestone ladder.

## Prerequisites

- Built emulator binary (see [`BUILDING.md`](BUILDING.md)).
- Licensed terminal firmware binary at `../firmware/flash.bin`.
- Optional: terminal front-panel image at `docs/images/terminal-unbranded.webp` for artwork rendering.
- Optional: a running CoinLine Host endpoint for the host-bridge connection (see [`docs/host-integration-plan.md`](docs/host-integration-plan.md)).

## Quick launch (MAME track — planned, tranche E2 onward)

```bash
./coinline-emu \
    -bios millennium \
    -rompath roms/ \
    -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -nvram fixtures/nvram/factory-default.nvram.json \
    -hostbridge tcp://127.0.0.1:5210 \
    -log run.log
```

Switches expected on the command line, finalized in [`docs/mame-driver-plan.md`](docs/mame-driver-plan.md):

| Switch | Purpose |
| ------ | ------- |
| `-firmware <path>` | Path to the terminal firmware binary (`../firmware/flash.bin`). |
| `-board <profile>` | Board profile JSON selecting 2-line vs 11-line VFD and per-revision device wiring. |
| `-nvram <image>` | Initial NVRAM image (factory-default, corrupt-checksum, or operator-supplied). |
| `-hostbridge <url>` | Host-bridge transport endpoint (`tcp://`, `ws://`, `pipe://`, `serial://`). |
| `-scenario <file>` | Optional scenario file to run automatically (see [`docs/scenario-runner.md`](docs/scenario-runner.md)). |
| `-evidence <dir>` | Output directory for evidence bundles (see [`docs/evidence-bundles.md`](docs/evidence-bundles.md)). |
| `-debug` | Enables MAME's debugger on startup. |
| `-log <file>` | Writes a structured run log including boot trace and I/O trace. |

## Quick launch (MIT-clean track — planned)

```bash
./coinline-emu \
    --firmware ../firmware/flash.bin \
    --board fixtures/board/board-profile-2line-vfd.json \
    --nvram fixtures/nvram/factory-default.nvram.json \
    --host-bridge tcp://127.0.0.1:5210 \
    --log run.log
```

The flag style differs but the semantics match the MAME track. Both tracks honor the same JSON board profiles and scenario files.

## Board profiles

Two reference profiles are described in [`docs/board-profiles.md`](docs/board-profiles.md):

- `board-profile-2line-vfd.json` — 2-line VFD variant.
- `board-profile-11line-vfd.json` — 11-line VFD variant.

Operators may add profiles for other revisions. The board profile selects:

- VFD variant and dimensions.
- Keypad layout (3x4 numeric, plus quick-access keys, volume, language).
- Card reader presence and type (magnetic, smart, both, or none).
- Coin validator configuration.
- Modem UART base port and IRQ.
- NVRAM size and base address.
- Firmware download staging area size.

## Connecting to CoinLine Host

The host bridge forwards modem UART bytes between the emulated terminal and an external endpoint. By default the emulator listens (or connects) using TCP at the address supplied to `-hostbridge`. See [`docs/modem-uart-host-bridge.md`](docs/modem-uart-host-bridge.md) for transport semantics, modem state model, and DCD/CTS/RTS/DTR behavior.

> The bridge does **not** synthesize protocol responses. It is a transparent byte pipe. Application-layer behavior comes from the real firmware on one side and CoinLine Host on the other.

## Scenarios and evidence bundles

To run a deterministic scenario for compatibility validation:

```bash
./coinline-emu \
    -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -scenario fixtures/scenarios/boot-to-idle.json \
    -evidence out/boot-to-idle/ \
    -log out/boot-to-idle/run.log
```

Scenario format is defined in [`docs/scenario-runner.md`](docs/scenario-runner.md) and [`specs/scenario-runner.spec.md`](specs/scenario-runner.spec.md). Evidence bundle contents are defined in [`docs/evidence-bundles.md`](docs/evidence-bundles.md) and [`specs/evidence-bundle.spec.md`](specs/evidence-bundle.spec.md).

## Front-panel artwork

When a front-panel image is provided at `docs/images/terminal-unbranded.webp`, the emulator overlays:

- A 2-line or 11-line VFD region (per board profile).
- Clickable keypad, quick-access keys, volume keys, language key, hookswitch, card slot, coin input, coin return, lock, door, vault, and service switch (per [`docs/artwork-and-layout.md`](docs/artwork-and-layout.md)).

If the image is absent, the emulator runs headless and exposes the same controls programmatically through scenarios.

## Headless operation

For CI and batch validation:

```bash
./coinline-emu \
    -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -scenario fixtures/scenarios/boot-to-idle.json \
    -nowindow -nosound \
    -evidence out/ \
    -exit-on-scenario-end
```

`-nowindow` and `-nosound` correspond to MAME's standard headless flags; the MIT-clean track accepts `--headless`.

## Logs and traces

Every run produces:

- A run log (`-log` / `--log`) with boot trace, I/O trace, UART transcript, NVRAM writes, host-bridge frames, and scenario events.
- An evidence bundle if `-evidence` is supplied.
- Optional MAME state snapshot files if `-savestate` is invoked from the debugger.

See [`docs/debugging-guide.md`](docs/debugging-guide.md) for guidance on inspecting logs and reproducing failures.

## Common issues

| Symptom | Likely cause | Fix |
| ------- | ------------ | --- |
| `firmware binary not found` | `../firmware/flash.bin` empty or wrong | Supply the path on the command line. |
| Emulator stops at M1 | Reset vector mismatch | Verify board profile and firmware version per ``. |
| No display output | VFD device disabled in board profile | Enable VFD in the board profile JSON. |
| Host bridge times out | CoinLine Host not running or wrong endpoint | Start CoinLine Host and adjust `-hostbridge`. |
| Many `unknown port` log lines | Expected during early bring-up | See ``. |
