# License strategy — CoinLine Terminal Emulator

This document explains the licensing posture of the **CoinLine Terminal Emulator**: which license it is distributed under, why that choice was made, and what operators and integrators need to understand before consuming or contributing.

## Summary

- The emulator is distributed under **GPL-2.0-or-later**. See [`LICENSE`](LICENSE).
- The default engine is **MAME**, which is itself GPL-2.0-or-later. The emulator's machine driver, devices, and supporting code are written to live inside a MAME source tree (`src/mame/coinline/`) and are compatible with that license.
- **Terminal firmware images are not bundled with releases of this project.** When you build a release tarball you should not include any firmware binary that you do not have the rights to redistribute. Operators supply their own licensed firmware binary at runtime via a path (see [`RUNNING.md`](RUNNING.md)).
- Reference fixture firmware images are included in this repository's `firmware/` directory **only** for the convenience of contributors who are reproducing emulator bring-up and acceptance work. They are not part of any release artifact and are not redistributed by this project's tarballs.
- Any process talking to the emulator over its host bridge (TCP, WebSocket, named pipe, or serial) is **not** statically or dynamically linked into the emulator binary and therefore is not subject to the emulator's GPL obligations.

## Why GPL

Because the engine of choice is MAME, the resulting binary inherits GPL-2.0-or-later distribution and source-availability obligations. Rather than fight that, the project embraces it: the machine driver, device models, fixtures, specs, test plans, and tooling are all designed to live alongside MAME as a regular MAME driver tree under [`src/mame/coinline/`](src/mame/coinline/).

If you build a derivative work that includes the emulator's compiled output (or a statically linked subset of its source), the GPL obligations follow that derivative work. The project's source tree, build scripts, and CI are all structured to make satisfying those obligations straightforward.

## Out-of-process integrations

The emulator exposes a **host bridge** that forwards the emulated terminal's modem UART byte stream over TCP, WebSocket, named pipe, or direct serial. Programs on the other side of the bridge run in a separate OS process and are not linked into the emulator binary. The bridge is the license firewall for any third-party host application: as long as the host application talks to the emulator only over the bridge, the host application's own license is not affected by GPL.

See [`docs/host-integration-plan.md`](docs/host-integration-plan.md) and [`docs/modem-uart-host-bridge.md`](docs/modem-uart-host-bridge.md) for the bridge protocol and supported transports.

## Firmware handling

- `firmware/` ships fixture firmware images used to reproduce the emulator's acceptance and bring-up runs. The list of currently committed images is enumerated in [`firmware/README.md`](firmware/README.md) (or in the directory listing). Distribution of those images is governed by their respective licenses, which is the operator's responsibility to ensure.
- The emulator's MAME driver loads firmware via a configurable path. The default lookup is `firmware/flash.bin` (with `firmware/flash1.bin` for split-image installs) and `firmware/voice_a.bin`, `firmware/voice_b.bin` for the voiceware ROMs.
- The emulator does not embed firmware contents into its own binary. Firmware-derived test fixtures use **SHA-256 manifests** ([`fixtures/voiceware/voice-rom-sha256.expected.json`](fixtures/voiceware/voice-rom-sha256.expected.json)) so that tests can verify firmware integrity without storing firmware contents in the test code.

## Alternative MIT-clean implementation track (planning placeholder)

If at any point the project pivots away from the GPL engine — for example to embed the emulator in a permissively-licensed product — the project's documentation, specs, fixtures, test plans, and scenario runner are designed to be engine-agnostic. Switching tracks would require:

- Replacing the MAME-based machine driver under `src/mame/coinline/` with a host application built against a permissively-licensed Z180 core and custom device emulation.
- Re-implementing the device models in this project's `src/` tree under the new license.
- Keeping the public-facing artifact set (`docs/`, `specs/`, `test-plans/`, `fixtures/`, `tools/`) unchanged.

This track is not currently active. The recommended engine is documented in [`docs/engine-selection.md`](docs/engine-selection.md).

## Contributing

Contributions are accepted under the project's GPL-2.0-or-later license. By submitting a pull request you agree to license your contribution under those terms. See `CONTRIBUTING.md` (if present) for style and review notes, and [`docs/`](docs/) for the architectural and testing material a contributor needs to make an informed change.
