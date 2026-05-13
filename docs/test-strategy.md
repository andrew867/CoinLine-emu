# Test strategy

This document describes how testing is organized for `coinline-emu`. It complements [`acceptance-test-plan.md`](acceptance-test-plan.md) and the per-area plans in [`../test-plans/`](../test-plans/).

## Goals

- Real firmware execution — every test that can use real firmware does. No high-level fakes that bypass the firmware code path.
- Determinism — every test produces identical results given the same inputs (firmware binary, board profile, NVRAM image, scenario, host-bridge transcript).
- Evidence-first — every acceptance and scenario test produces an evidence bundle ([`evidence-bundles.md`](evidence-bundles.md)) attached to PRs that claim closure.
- Fast inner loop — unit and device tier tests run in milliseconds; CI surfaces failures within minutes.
- Public/internal separation — gated optional tests live in `tests/internal/`, are not part of the public CI matrix, and run only when the operator enables the firmware-checkout prerequisites documented there.

## Tiers

| Tier | Goal | Latency | Owners |
| ---- | ---- | ------- | ------ |
| Unit | Test a single device or helper in isolation; no firmware. | < 100 ms each | Device authors |
| Device | One device wired into a minimal machine harness; firmware-driver–level coverage; uses a small purpose-built ROM stub or the real firmware ROM with strict cycle bounds. | < 5 s each | Device authors |
| Integration | Multiple devices, real firmware, deterministic boot to a known PC or VFD state. | 5–30 s each | Emulator engineers |
| Boot | Full firmware boot; verify boot milestones M0–M10 reached. | 30–120 s | Emulator engineers |
| Scenario | Full firmware run, scenario-driven; produces evidence bundle. | 5–60 s | QA / validation |
| Acceptance | Scenario suite + CoinLine Host integration. | 1–10 min | QA / validation |
| Regression | Saved-state replay against golden traces. | Variable | Maintainers |

## Test sources

| Source | What it provides |
| ------ | ---------------- |
| Real firmware binary | The authoritative source of behavior. Used in device, integration, boot, scenario, acceptance, regression. |
| Fixtures (`fixtures/board/*.json`) | Memory map, I/O port map, interrupt map, device map, board profiles. |
| Fixtures (`fixtures/nvram/*.json`) | Factory + corrupt-checksum NVRAM images. |
| Fixtures (`fixtures/cards/*.json`) | Magnetic and smart-card payloads. |
| Fixtures (`fixtures/modem/*.hex`) | Modem byte streams for state-machine tests. |
| Fixtures (`fixtures/scenarios/*.json`) | Scripted scenarios. |
| Fixtures (`fixtures/display/*.json`) | Expected VFD outputs. |
| Golden traces | Recorded evidence bundles used as regression baselines. |

## Naming and organization

```
tests/
  fixtures/             # schema validation tests for fixtures and specs
  boot/                 # boot-tier tests
  devices/              # unit + device-tier tests
  integration/          # integration + scenario + acceptance + regression
  internal/             # optional gated tests; not part of public CI
```

Each test file's first comment block names the test plan it implements (e.g., `// Implements: test-plans/keypad-tests.md` lines/section ids).

## Test-data integrity

- Firmware binary content is not committed; tests verify the operator's binary against a SHA-256 hash recorded in `fixtures/firmware/firmware-hashes.json`.
- Tests are explicit about which firmware version they target. A test that requires a specific firmware version skips (with a clear message) when run against a non-matching binary.

## Determinism rules

- No wall-clock timing in tests. Every step uses Z180 cycles.
- No PRNG seeded from time. Pseudo-random sources accept an explicit seed.
- Host-bridge transcripts are recorded and replayed verbatim in deterministic tests.
- Save state is used to skip slow boot phases when not under test (the boot phase is itself tested elsewhere).

## Coverage targets

| Layer | Target |
| ----- | ------ |
| Z180 internal peripherals | 100% of registers exercised by firmware boot. |
| Devices (per-device specs) | 100% of declared I/O addresses, 100% of declared state-machine transitions. |
| Scenarios | One scenario per major user flow (boot, keypad, modem connect, table download, card call, coin call, service mode). |
| Boot milestones | M0 through M10 covered by `tests/boot/`; M11 and M12 covered by host-integration tests. |
| Risk register | Every `risk-register.md` row points to at least one test. |
| Compatibility validation | Every `compatibility-validation-items.md` row references at least one test or evidence artifact. |

## What we do **not** test

- We do not unit-test MAME's Z180 core itself; we treat it as a dependency. Bugs found in the core are reported upstream.
- We do not test physical hardware behavior in CI. Field validation is a separate process documented in [`compatibility-validation-items.md`](compatibility-validation-items.md).
- We do not test CoinLine Host's internal logic from this side; integration tests verify only that bytes flow correctly across the host bridge.

## Failure handling

- Test failure produces:
  - The CI log with the assertion message.
  - The evidence bundle (for tests that emit one).
  - A note in the relevant compatibility-validation row marking the item as `regressed` (humans triage).
- Tests that depend on `../firmware/flash.bin` or `../firmware source tree` skip cleanly (with a documented skip reason) when the input is missing — they never silently pass.

## CI matrix

Per [`ci-and-release.md`](ci-and-release.md):

| Job | OS | Configuration | Tier |
| --- | -- | ------------- | ---- |
| `unit-linux` | Linux | Debug | Unit + device |
| `unit-windows` | Windows | Debug | Unit + device |
| `integration-linux` | Linux | RelWithDebInfo | Boot + integration + scenario |
| `nightly-acceptance` | Linux | Release | Acceptance + regression |
| `nightly-host-integration` | Linux | Release | Host integration against CoinLine Host |

## Cross-references

- [`acceptance-test-plan.md`](acceptance-test-plan.md) — acceptance gate.
- [`evidence-bundles.md`](evidence-bundles.md) — evidence bundle schema.
- [`scenario-runner.md`](scenario-runner.md) — scenario verbs.
- [`debugging-guide.md`](debugging-guide.md) — what to do when a test fails.

