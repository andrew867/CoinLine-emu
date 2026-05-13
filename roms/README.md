# `roms/`

This directory is the conventional location for operator-supplied terminal firmware binaries that the emulator loads at runtime.

## Important

- **Firmware binaries are not redistributed with this project.** Do not commit firmware binaries to this repository. They are excluded by `.gitignore` (added in tranche E0).
- Firmware binaries must be licensed for the operator's use. CoinLine Terminal Emulator does not provide them.
- The emulator's runtime resolves the firmware binary via the `-firmware <path>` (MAME track) or `--firmware <path>` (clean-room track) CLI flag. The path can point anywhere on disk; this directory is just the conventional default.

## Expected contents at runtime

| File | Purpose |
| ---- | ------- |
| `<firmware>.bin` | Firmware binary loaded into the ROM region. Path corresponds to `../firmware/flash.bin`. |
| `firmware-hashes.json` | (Optional alternative location) SHA-256 hash registry per [`../specs/firmware-loader.spec.md`](../specs/firmware-loader.spec.md). The committed registry actually lives at `fixtures/firmware/firmware-hashes.json`. |

## Cross-references

- [`../RUNNING.md`](../RUNNING.md) — how to point the emulator at a firmware binary.
- [`../specs/firmware-loader.spec.md`](../specs/firmware-loader.spec.md) — loader contract and hash verification.
- [`../docs/firmware-download-storage.md`](../docs/firmware-download-storage.md) — DLA staging.
- [`../LICENSE-STRATEGY.md`](../LICENSE-STRATEGY.md) — firmware-handling rules.
