# Build plan

This document describes how the build system rolls out across tranches. The build is intentionally absent in the docs-only pass and is introduced incrementally so that license isolation is provable from day one.

## Phases

### Phase 1 — Skeleton (E0)

- A `LICENSE` file appropriate to the engine track.
- A `.gitignore` that excludes firmware extensions and build outputs.
- For the MAME track, a thin `scripts/build-mame.sh` (and `scripts/build-mame.ps1`) that drives the external MAME tree.
- For the MIT-clean track, a `CMakeLists.txt` with no source files but a `target_sources()` ready to populate.

### Phase 2 — Skeleton compiles (E2)

- All MAME machine driver sources compile clean.
- Boot trace runs.
- The emulator binary runs `--list-machines` and prints the CoinLine driver short-name.

### Phase 3 — Z180 + devices wired (E3–E9)

- Devices are added one tranche at a time.
- Each tranche adds at least one CTest label and a corresponding fixture schema.

### Phase 4 — Tooling (E11)

- `tools/` binaries are added under their own targets.
- Tools may share helper code via the `coinline_emu_common` static library (still GPL or still MIT depending on track — never crossing the tracks).

### Phase 5 — Hardening (E13)

- Release configurations.
- Strip + symbol-package separation.
- Cross-platform CI matrix.

## Toolchains

| Track | Toolchain |
| ----- | --------- |
| MAME | The MAME-supported toolchains: clang/gcc on Linux/macOS, MSVC + MinGW on Windows. |
| MIT-clean | C++17 with CMake 3.20+; same OS coverage. |

## Binary linking boundary (license isolation)

Keeping emulator outputs (GPL-linked) and backend outputs cleanly separated is the operational test of license isolation:

- `coinline-emu`'s emulator binary links against MAME (GPL) and project-local sources only.

Each side's build invocations are visible in CI logs; reviewers can verify isolation by inspection.

## Reproducibility

- The MAME track records the MAME revision used.
- The MIT-clean track records the Z80 core revision used (if any) and the C++ standard library.
- Build outputs are reproducible in the sense that two clean checkouts on the same OS/toolchain produce binaries that pass the same scenarios with the same evidence bundles.

## Artifacts

| Artifact | Phase | Notes |
| -------- | ----- | ----- |
| Emulator binary | 2 | Distributed only under the chosen track's license. |
| Tool binaries | 4 | Distributed alongside the emulator binary. |
| Test binaries | 2 | Internal; not shipped. |
| Documentation site | n/a | Not part of this project. |

## Cross-references

- [`engine-selection.md`](engine-selection.md) — chosen engine.
- [`project-structure.md`](project-structure.md) — directory layout.
- [`ci-and-release.md`](ci-and-release.md) — CI matrix and release artifacts.
