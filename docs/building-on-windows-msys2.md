# Windows MAME toolchain

The verified Windows build uses MSYS2 **MINGW64** and the repository’s pinned MAME checkout. The toolchain is for developers building the emulator. The portable ZIP contains the application runtime.

## Prepare the checkout

From PowerShell in `coinline-emu`:

```powershell
.\tools\windows\bootstrap-msys2-mame.ps1
```

The bootstrap script prepares the MINGW64 build dependencies and MAME checkout under `third-party/mame`. An existing checkout can be supplied with the script’s `-MameRoot` argument. Use the checkout pinned by `config/mame-pinned.ref` or the existing recorded bootstrap revision.

## Build with the overlay

```powershell
.\tools\windows\build-mame-coinline.ps1
```

The wrapper applies the CoinLine driver, layout, and Z180 ASCI interrupt correction, runs the native MAME build in MINGW64, and writes `build/bin/coinline-mame.exe`. The system name is `cl_millennium`.

For a portable build, start the **MSYS2 MINGW64** shell, change to `coinline-emu`, and run:

```bash
/usr/bin/python tools/mingw64/build-portable.py
```

See [build and packaging options](../BUILDING.md), [launch instructions](../RUNNING.md), and [verification](../TESTING.md).
