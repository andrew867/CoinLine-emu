# CI and release

## Checked-in CI

The public repository runs the C++ emulator tests and fixture validation. Licensed firmware and SIP configuration are operator inputs for lab acceptance runs.

**Native emulator builds** compiles the pinned MAME subtarget on Windows x64, Ubuntu Linux x64, and macOS on Apple Silicon. Each job launches its packaged executable with `-listfull cl_millennium` and uploads an archive and SHA-256 checksum. The Windows package also runs the SIP loopback suite with its embedded runtime. Separate Linux/macOS host-media jobs exercise the real SIP runtime and the native PortAudio capture adapter against a deterministic host fixture. Pull requests and pushes to `main` produce build artifacts under Actions. A `v*` tag publishes the three packages and a matching CoinLine and MAME source archive after the native and host-media matrices pass. Physical microphone access and external-provider calls are separate operator acceptance tests.

Public packages require operator-supplied firmware and voice ROMs. The source archive contains the pinned MAME source and the CoinLine overlay and build scripts. Extract both source folders and pass `--mame-root ../mame` from the `coinline` folder to reproduce the build on the selected platform.

## Windows local artifact

Run [the portable builder](../BUILDING.md) from MSYS2 MINGW64. Its Windows x64 ZIP includes the emulator, entry points, artwork, local CO sidecar, Python and SIP dependencies, and the quick start. The archive has a SHA-256 sidecar and an internal file manifest. Local ROMs are included only by explicit input and are marked in the package name.

Before handing over a ZIP, extract it to a new writable path. Verify the manifest, run the OEM launcher's `--check`, and perform the headless boot smoke test. Test the windowed launch and retain its log. See [testing](../TESTING.md) for how the package smoke differs from the firmware acceptance.

## Firmware and release evidence

The [firmware acceptance handoff](status/ncc-install-firmware-stx-handoff-2026-11-23.md) covers both complete INSTALL flows, restart, the answered demo SIP call, and the OEM spoken rate. Keep raw run captures with local build outputs. A distribution outside the local workspace requires the applicable MAME source and license terms. Keep local ROM images, credentials, and battery images out of a shareable artifact.
