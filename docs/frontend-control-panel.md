# Frontend control panel

This document describes an optional control-panel UI overlay that supplements the MAME `.lay` artwork with developer-oriented controls.

## Purpose

- Provide quick access to scenario steps without writing JSON.
- Display live device traces (VFD buffer, modem state, NVRAM diff, host-bridge transcript).
- Allow on-the-fly fixture injection (swipe a card, insert a coin) from a panel separate from the front-panel artwork.

## Status


## Components

| Component | Purpose |
| --------- | ------- |
| Trace viewer | Tail boot trace, I/O trace, UART transcript. |
| Device inspector | Show per-device state, snapshot, last write. |
| Fixture launcher | Drop-down of `fixtures/cards/*`, `fixtures/modem/*`, `fixtures/scenarios/*`. |
| Scenario player | Step-by-step scenario advance / rewind. |
| Evidence bundle picker | Browse and replay an existing bundle. |

## Integration

- The MAME track adds the control panel via MAME's plugin / Lua-script mechanism.
- The MIT-clean track may use a small native overlay or no panel at all.
- Either way, the panel reads from the same trace stream the evidence bundle exporter consumes.

## License

The control panel inherits the engine track's license. It does not link any `coinline/` source.

## Cross-references

- [`debugging-guide.md`](debugging-guide.md).
- [`scenario-runner.md`](scenario-runner.md).
- [`evidence-bundles.md`](evidence-bundles.md).
- [`artwork-and-layout.md`](artwork-and-layout.md).
