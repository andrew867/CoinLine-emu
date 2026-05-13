# CI and release

This document describes the CI matrix and release artifacts for `coinline-emu`.

## CI matrix

| Job | OS | Configuration | Purpose | Tier |
| --- | -- | ------------- | ------- | ---- |
| `unit-linux` | Ubuntu LTS | Debug | Unit + device tests | All PRs |
| `unit-windows` | Windows Server | Debug | Unit + device tests | All PRs |
| `integration-linux` | Ubuntu LTS | RelWithDebInfo | Boot + integration + scenarios | All PRs |
| `nightly-acceptance` | Ubuntu LTS | Release | Full acceptance suite | Nightly |
| `nightly-host-integration` | Ubuntu LTS | Release | CoinLine Host integration | Nightly |
| `regression-replay` | Ubuntu LTS | Release | Saved-state replay against golden traces | Nightly |

## Required jobs (must be green for merge)

- `lint`
- `unit-linux`
- `unit-windows`
- `integration-linux`

## Release artifacts

Per release:

- Emulator binary (Linux / Windows / macOS where supported).
- Tool binaries (`boot-trace-parser`, `io-trace-analyzer`, `evidence-bundle-export`).
- `LICENSE`, `LICENSE-STRATEGY.md`, `README.md`, `BUILDING.md`, `RUNNING.md`, `TESTING.md`.
- SHA-256 checksums of every artifact.
- Optional: a source tarball under GPL-2.0-or-later.

## Release procedure

1. Tag the release in git.
2. Trigger the release workflow.
3. The workflow runs `lint`, `unit-linux`, `unit-windows`, `integration-linux`, and `nightly-acceptance` against the tag.
4. On success, the workflow packages artifacts and produces an evidence bundle of the canonical scenario suite.
5. Reviewer attaches the evidence bundle to the release.

## License footers in releases

Release notes include the GPL-2.0-or-later distribution notice and a pointer to the source tarball.

## Versioning

`MAJOR.MINOR.PATCH` semantic versioning. Versions follow the firmware-target compatibility table in [`compatibility-validation-items.md`](compatibility-validation-items.md): a `MINOR` bump when a new firmware version becomes supported; a `PATCH` bump for fixes.

## Cross-references

- [`build-plan.md`](build-plan.md) — what the CI builds.
- [`acceptance-test-plan.md`](acceptance-test-plan.md) — what `nightly-acceptance` runs.
