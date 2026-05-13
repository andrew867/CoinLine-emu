# Building CoinLine Terminal Emulator

This document describes the build prerequisites, supported toolchains, and command sequences for building `coinline-emu`. No build system is materialized yet — these instructions are forward-looking and apply once tranches E0–E2 are implemented.

## Prerequisites

| Tool | Minimum version | Notes |
| ---- | --------------- | ----- |
| C++ compiler | C++17 (clang 13+, gcc 10+, MSVC 19.30+) | MAME requires C++17. |
| Python | 3.10+ | Build scripts and fixture tooling. |
| Git | Recent | Submodule support if MAME is used as a submodule. |
| GNU make / msys2 (Windows) | Recent | MAME's primary build system. |
| SDL2 / SDL_ttf / SDL_image | Recent | Required by MAME for UI/artwork rendering. |
| pkg-config | Recent | MAME dependency discovery on Linux/macOS. |

If the **MIT-clean fallback track** ([`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md) → *Alternative MIT-clean implementation track*) is selected, the prerequisites simplify to a C++17 compiler and CMake 3.20+.

## License posture

Before running any build, confirm:

- You are operating under the rules in [`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md).

## MAME-based track (default)

The MAME-based build is structured per [`docs/mame-driver-plan.md`](docs/mame-driver-plan.md) and [`docs/project-structure.md`](docs/project-structure.md). The high-level steps are:

1. Obtain the MAME source tree under a path **outside this repository** — for example, alongside this project at `third-party/mame/`. Do not commit MAME source into this repository unless explicitly authorized.
2. Place CoinLine machine driver sources at the location specified by [`docs/file-plan.md`](docs/file-plan.md) → `src/mame/coinline/` (planned).
3. Register the driver in MAME's driver list per [`docs/mame-driver-plan.md`](docs/mame-driver-plan.md).
4. Build MAME with the CoinLine driver included.

Example command sketch (Linux/macOS, planned — exact recipe finalized in tranche E2):

```bash
cd third-party/mame
make SUBTARGET=coinline SOURCES=src/mame/coinline/millennium.cpp -j$(nproc)
```

The resulting binary is named `coinline` (or `coinline.exe`) and is documented in [`RUNNING.md`](RUNNING.md).

## MIT-clean fallback track

If [`docs/engine-selection.md`](docs/engine-selection.md) records a switch to the MIT-clean track, the build uses CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

The CMake project is structured per [`docs/project-structure.md`](docs/project-structure.md) .

## Verifying the build

After a successful build:

- The emulator binary exists in the location reported by the build.
- Running `<emulator> --version` prints the emulator version.
- Running `<emulator> --list-machines` (MAME) lists `millennium` (or the chosen short-name from [`docs/mame-driver-plan.md`](docs/mame-driver-plan.md)).
- Running [`TESTING.md`](TESTING.md) commands does not error out on environment misconfiguration.

## Build artifacts

| Artifact | Location | Notes |
| -------- | -------- | ----- |
| Emulator binary | `build/coinline` (or platform equivalent) | Distributed only under the GPL track's source-availability terms (or under MIT terms in the clean-room track). |
| Tool binaries | `build/tools/` | Boot trace parser, I/O trace analyzer, evidence bundle exporter (tranche E13). |
| Test binaries | `build/tests/` | Unit, device, integration suites. |
| Generated docs | n/a | None at build time — documentation is hand-authored. |

## CI builds

CI configuration is described in [`docs/ci-and-release.md`](docs/ci-and-release.md). The CI matrix includes Linux and Windows hosts in debug and release configurations.

## Troubleshooting

| Symptom | Likely cause | Fix |
| ------- | ------------ | --- |
| `fatal error: SDL.h: No such file or directory` | SDL2 dev package missing | Install SDL2/SDL_ttf/SDL_image dev packages. |
| Linker error referencing `coinline_*` symbols | Out-of-tree host sources accidentally linked into the emulator binary | Remove any non-emulator source paths from include/link lists; see [`LICENSE-STRATEGY.md`](LICENSE-STRATEGY.md). |
| Driver not discovered | Driver not registered in MAME driver list | Update the driver list per [`docs/mame-driver-plan.md`](docs/mame-driver-plan.md). |
| `roms/` empty | Firmware binary not supplied | Populate `../firmware/flash.bin` per [`RUNNING.md`](RUNNING.md). |
