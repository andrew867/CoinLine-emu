# Testing CoinLine Terminal Emulator

This document is the entry point for everyone running tests against `coinline-emu`. The full per-area test plans live under [`test-plans/`](test-plans/), and the global test strategy lives at [`docs/test-strategy.md`](docs/test-strategy.md).

## Test taxonomy

| Tier | Scope | Speed | Run on every push? |
| ---- | ----- | ----- | ------------------ |
| Unit | Single device, in isolation, no firmware | Sub-second | Yes |
| Device | One device wired into the machine, micro-firmware stub | Seconds | Yes |
| Integration | Multiple devices, firmware-driven from a known boot ROM stub | Seconds–minutes | Yes |
| Boot | Real firmware loaded; advance to a target milestone | Seconds–minutes | Nightly |
| Scenario | JSON scenario; full deterministic flow | Seconds | Per-PR + nightly |
| Acceptance | Full firmware, scenario suite, host-bridge against CoinLine Host | Minutes | Nightly + release |
| Regression | Saved-state + golden trace replay | Variable | Nightly |

Each tier is described in detail in [`docs/test-strategy.md`](docs/test-strategy.md), and per-area procedures, fixtures, and pass/fail criteria are in [`test-plans/`](test-plans/).

## Real MAME + firmware boot (mandatory for milestone closure)

**These tests launch the MAME-based CoinLine Terminal Emulator, load the configured firmware binary, execute real Z180 firmware code, and validate boot milestone artifacts emitted by the emulator** — when you run:

```powershell
.\tools\windows\test-coinline-emulator.ps1 -FirmwareBinary '<absolute path to flash.bin>'
```

That path chains `run-coinline-emulator.ps1` (spawns `mame.exe` with system name **`millennium`**) and `validate-boot-milestones.ps1` (reads real `boot-trace.jsonl`, not scenario JSON), then the PowerShell files under `tests/integration/*.Tests.ps1`.

CMake targets such as `test_boot_to_idle` / `test_evidence_bundle` exercise **scenario JSON and the evidence-bundle writer** only. They **do not** launch MAME and **cannot** close M0–M10 against live firmware. Treat them as preconditions for automation, not as firmware boot proof.

CI without a licensed ROM may pass `-SkipIfNoFirmware` on `test-coinline-emulator.ps1` (exit **77**). **Do not** treat that skip as boot validation success for release acceptance.

See [`docs/building-on-windows-msys2.md`](docs/building-on-windows-msys2.md) for MSYS2 + MAME build steps.

## Quick command sheet

Boot / unknown-port C++ tests (CMake + Ninja or your generator):

```bash
cmake -S coinline-emu -B build
cmake --build build
ctest --test-dir build --label-regex "boot|unknown_port" --output-on-failure
ctest --test-dir build --label-regex z180 --output-on-failure
```

`test_firmware_load` returns skip code **77** when `../firmware/flash.bin` is absent (CI machines without the operator firmware).

Reserved invocations for additional tiers (still rolling out):

```bash
ctest --test-dir build --label-regex unit
ctest --test-dir build --label-regex device
ctest --test-dir build --label-regex integration
ctest --test-dir build --label-regex scenario
ctest --test-dir build --label-regex acceptance
ctest --test-dir build --label-regex regression
```

The MAME track may use additional MAME-side regression invocations (saved state replay), documented in [`docs/test-strategy.md`](docs/test-strategy.md).

## Fixtures

All fixtures are described in [`docs/file-plan.md`](docs/file-plan.md) and live under [`fixtures/`](fixtures/). Categories:

- `fixtures/board/` — memory map, I/O port map, interrupt map, device map, board profiles.
- `fixtures/firmware/` — firmware binary metadata (hashes only — no firmware content).
- `fixtures/nvram/` — factory-default and corrupt-checksum NVRAM images.
- `fixtures/cards/` — magnetic and smart-card payloads, valid and invalid.
- `fixtures/modem/` — modem byte streams (clean connect, dropped carrier, noisy line).
- `fixtures/scenarios/` — JSON scenario files (boot-to-idle, keypad smoke, modem connect, table download, card call, coin call, service mode).
- `fixtures/display/` — expected VFD outputs (2-line idle, 11-line ad).

Fixtures are content-addressed by SHA-256; tests verify the operator's firmware binary against `fixtures/firmware/firmware-hashes.json` to ensure correct firmware-version coverage.

## Test plans

| Test plan | File |
| --------- | ---- |
| Boot tests | [`test-plans/boot-tests.md`](test-plans/boot-tests.md) |
| CPU and Z180 register tests | [`test-plans/cpu-and-z180-register-tests.md`](test-plans/cpu-and-z180-register-tests.md) |
| Memory map tests | [`test-plans/memory-map-tests.md`](test-plans/memory-map-tests.md) |
| I/O port tests | [`test-plans/io-port-tests.md`](test-plans/io-port-tests.md) |
| VFD tests | [`test-plans/vfd-tests.md`](test-plans/vfd-tests.md) |
| Keypad tests | [`test-plans/keypad-tests.md`](test-plans/keypad-tests.md) |
| Hookswitch tests | [`test-plans/hookswitch-tests.md`](test-plans/hookswitch-tests.md) |
| Card reader tests | [`test-plans/card-reader-tests.md`](test-plans/card-reader-tests.md) |
| Smart-card tests | [`test-plans/smartcard-tests.md`](test-plans/smartcard-tests.md) |
| Coin validator tests | [`test-plans/coin-validator-tests.md`](test-plans/coin-validator-tests.md) |
| Modem UART tests | [`test-plans/modem-uart-tests.md`](test-plans/modem-uart-tests.md) |
| NVRAM storage tests | [`test-plans/nvram-storage-tests.md`](test-plans/nvram-storage-tests.md) |
| Firmware download tests | [`test-plans/firmware-download-tests.md`](test-plans/firmware-download-tests.md) |
| Table download tests | [`test-plans/table-download-tests.md`](test-plans/table-download-tests.md) |
| Host integration tests | [`test-plans/host-integration-tests.md`](test-plans/host-integration-tests.md) |
| Scenario runner tests | [`test-plans/scenario-runner-tests.md`](test-plans/scenario-runner-tests.md) |
| Artwork layout tests | [`test-plans/artwork-layout-tests.md`](test-plans/artwork-layout-tests.md) |
| Regression tests | [`test-plans/regression-tests.md`](test-plans/regression-tests.md) |
| Acceptance tests | [`test-plans/acceptance-tests.md`](test-plans/acceptance-tests.md) |

## Evidence bundles

Every acceptance and scenario test produces an evidence bundle. The schema is defined in [`docs/evidence-bundles.md`](docs/evidence-bundles.md) and [`specs/evidence-bundle.spec.md`](specs/evidence-bundle.spec.md). Evidence bundles are the single artifact attached to PRs that claim compatibility validation closure.

## What "passing" means

Per [`docs/compatibility-validation-items.md`](docs/compatibility-validation-items.md):

- **Code implementation complete** — the emulator implements the behavior under test, and tests pass against fixtures.
- **Emulator validation complete** — the emulator's behavior matches expectations across the full scenario suite.
- **Physical hardware certification pending** — independent of the emulator; never blocks code merging.

## Running tests against CoinLine Host

The host integration tests in [`test-plans/host-integration-tests.md`](test-plans/host-integration-tests.md) bring up a real CoinLine Host process and exercise the emulator's modem UART path against it. Setup:

```bash
# In one shell — start CoinLine Host (MIT)
# launch your host application here
dotnet run --project src/HostPlatform.Api --launch-profile http

# In another shell — run emulator integration tests
ctest --test-dir build --label-regex integration -R host_bridge
```

Host integration tests communicate over the host bridge transport only; **no source-level coupling** between `coinline/` and `` is involved.

## Reporting issues

When a test fails:

1. Collect the evidence bundle.
2. Note the firmware binary SHA-256 hash and board profile.
3. Open an issue with the bundle attached and reference the relevant test-plan file.
