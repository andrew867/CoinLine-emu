# `tests/`

This directory holds the automated test corpus. The taxonomy and global strategy are in [`../docs/test-strategy.md`](../docs/test-strategy.md); per-area plans are in [`../test-plans/`](../test-plans/).

## Subdirectories (planned)

| Subdirectory | Purpose | Tranche |
| ------------ | ------- | ------- |
| `boot/` | Boot-tier tests (firmware load, reset vector, first instructions, RAM init, RTOS entry). | E2 onward |
| `devices/` | Unit + device-tier tests for every device in [`../docs/device-model.md`](../docs/device-model.md). | E2 onward |
| `integration/` | Integration / scenario / acceptance / regression tests. | E6 onward |
| `fixtures/` | Schema validation tests for every fixture and spec under [`../fixtures/`](../fixtures/) and [`../specs/`](../specs/). | E1 onward |
| `internal/` | Tests that depend on a pinned firmware tree outside `fixtures/`; not part of the public CI matrix. | E1 onward |

## Conventions

- Each test file's first comment block names the test plan it implements (e.g. `// Implements: test-plans/keypad-tests.md`).
- Tests use Z180 cycles, never wall-clock time.
- Integration tests that require CoinLine Host are tagged with the CTest label `host_integration`.

## Cross-references

- [`../docs/test-strategy.md`](../docs/test-strategy.md).
- [`../docs/acceptance-test-plan.md`](../docs/acceptance-test-plan.md).
- [`../test-plans/`](../test-plans/).
