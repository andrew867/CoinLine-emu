# Project structure

This document describes the planned source-tree, fixtures, artwork, tools, and tests layout for `coinline-emu`. Files marked **planned** are described here and tracked in [`file-plan.md`](file-plan.md), but are not materialized in this documentation pass.

``````

## Why this layout

- **Public docs first.** [`README.md`](../README.md), [`LICENSE-STRATEGY.md`](../LICENSE-STRATEGY.md), [`BUILDING.md`](../BUILDING.md), [`RUNNING.md`](../RUNNING.md), and [`TESTING.md`](../TESTING.md) are at the top level so newcomers can orient themselves immediately.
- **Specs vs docs.** Narrative documentation lives in `docs/`. Machine-readable contracts (port maps, scenario verbs, evidence bundle schema, device-by-device behavior) live in `specs/` so tools and tests can consume them.
- **Test plans vs tests.** `test-plans/*.md` describe what is tested and why. `tests/**` (planned) implement those plans.
- **Engine-track separation.** MAME-track source is under `src/mame/coinline/`. Clean-room source, if the fallback track is chosen, lives under `src/cleanroom/`. Both build into the same emulator binary; only one track is active at a time.
- **Fixtures are data.** Everything in `fixtures/` is JSON or hex bytes — no compiled code. This keeps fixtures portable across the engine tracks.
- **No firmware in the repo.** `roms/` contains only a `README.md` describing how to populate it. Firmware extensions are in the project `.gitignore` (added in tranche E0).

## File ownership

| Path | Tranche | Owners |
| ---- | ------- | ------ |
| `README.md`, top-level | E0 | Maintainers |
| `LICENSE-STRATEGY.md` | E0 | Maintainers |
| `docs/`, `specs/`, `test-plans/` | E0 | Maintainers + contributors |

| `src/mame/coinline/*` | E2 onward | Emulator engineers |
| `artwork/*` | E4 / E14 | Emulator engineers |
| `fixtures/board/*` | E1 / E2 | Emulator engineers |
| `fixtures/scenarios/*` | E11 | QA / validation |
| `tools/*` | E13 | Tooling engineers |
| `tests/**` | Per tranche | Test authors |

## Naming conventions

- C++ namespaces: `coinline::emu::*`.
- File prefix: `millennium_` for all per-device source files in the MAME track.
- Board profile filenames: `board-profile-<short>.<variant>.json` (e.g. `board-profile-2line-vfd.json`).
- Scenario filenames: kebab-case verbs describing the scenario (e.g. `boot-to-idle.json`).
- Fixture filenames: `<area>-<variant>.<ext>` (e.g. `magcard-valid.json`, `magcard-invalid-lrc.json`).

## What is **not** in this layout

- No `web/` — this project does not ship a web frontend. Front-panel rendering is via MAME artwork or, in the clean-room track, a native overlay.
- No `backend/` or `api/` — there is no service surface in this project. The host bridge is a transport, not an HTTP API.

- No `LICENSE` file at the project root in this pass — the license is documented in [`LICENSE-STRATEGY.md`](../LICENSE-STRATEGY.md) and applied per-source-file. A formal `LICENSE` file is added when implementation begins (tranche E0 implementation prompt).
