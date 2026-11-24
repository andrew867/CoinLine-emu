# Build and package the emulator

The public repository builds Windows x64, Ubuntu Linux x64, and macOS Apple Silicon packages in GitHub Actions. See [CI and release](docs/ci-and-release.md) for artifact and source downloads. SIP audio and microphone capture are available in the Windows build. Linux and macOS packages use the local central office and support terminal audio playback.

Before packaging from source, [assemble the original telephony controller model](tools/tp8048/). The CI workflow assembles this model and adds it to each package.

## Windows x64

The OEM Windows x64 package uses the MAME subtarget `coinline` and machine `cl_millennium`. Build it from an MSYS2 **MINGW64** shell. The existing bootstrap script prepares the compiler, build tools, and MAME checkout. Open the shell and change to the `coinline-emu` directory before running the commands below.

### Build a ZIP

```bash
/usr/bin/python tools/mingw64/build-portable.py
```

This builds the driver and prepares a ZIP in `build/dist/`, with its SHA-256 sidecar. Each invocation has a separate work directory and build log. It uses the MAME checkout under `third-party/mame`; pass `--mame-root` for a different prepared checkout. To package an existing Windows x64 CoinLine MAME binary, pass `--mame-exe /path/to/coinline-mame.exe`.

The package includes the launcher, front panel, telephony-processor image, card fixture, embedded Python, and central-office and SIP software. On the first build, the packager downloads pinned Windows Python and SIP wheels into `build/package-cache/` and checks their digests. Future builds can use that cache offline. It records the MAME base revision and hashes the packaged files.

## Linux x64 and macOS Apple Silicon

Use the MAME revision in `config/mame-pinned.ref`. On Ubuntu 24.04 install `build-essential`, `python3`, `libsdl2-dev`, `libsdl2-ttf-dev`, `libfontconfig1-dev`, `libxi-dev`, and `libxinerama-dev`. On macOS 15 install Xcode Command Line Tools and the Homebrew `python`, `pkgconf`, and `sdl3` packages.

```bash
python3 tools/mingw64/build-portable.py --mame-root /path/to/mame
```

The build produces a `.tar.gz` package and a SHA-256 checksum. Use Python 3.11 or newer to build the packages.

## Include your local ROM inputs

By default the ZIP leaves the terminal firmware and Voiceware files for the operator to add. To make a local, ready-to-launch copy, prepare a private input directory using these names and pass its path explicitly.

| File under the input directory | Size |
| --- | --- |
| `firmware/oem/flash.bin` | 256 KiB |
| `roms/cl_millennium/voice_u16.bin` | 1 MiB |
| `roms/cl_millennium/voice_u26.bin` | 1 MiB |

```bash
/usr/bin/python tools/mingw64/build-portable.py --rom-input /path/to/local-roms
```

This variant has `-local-roms` in its ZIP name. Keep it private and use only ROM images you are authorized to use. The OEM profile starts with empty battery state. The package carries no credentials or terminal data from the build machine.

## Build the executable only

From PowerShell, the existing wrapper runs the compile inside MSYS2/MINGW64.

```powershell
.\tools\windows\build-mame-coinline.ps1
```

It places the binary at `build/bin/coinline-mame.exe`. See [toolchain setup](docs/building-on-windows-msys2.md), [runtime](RUNNING.md), and [tests](TESTING.md).
