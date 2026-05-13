# I/O port map

This document describes the I/O port layout for the emulator. Per-port entries are pinned in `fixtures/board/io-port-map.json` (schema in [`../specs/io-port-map.spec.md`](../specs/io-port-map.spec.md)).

**Audio / supervision:** Port ownership for voiceware, routing, and supervision is additionally tracked in [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json), [`voiceware-command-map.json`](../fixtures/board/voiceware-command-map.json), [`audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json), and [`disconnect-supervision-map.json`](../fixtures/board/disconnect-supervision-map.json). MAME implementation bindings are specified in [`specs/voiceware-mame-device-implementation.spec.md`](../specs/voiceware-mame-device-implementation.spec.md), [`specs/audio-routing-mame-device-implementation.spec.md`](../specs/audio-routing-mame-device-implementation.spec.md), [`specs/disconnect-supervision-mame-device-implementation.spec.md`](../specs/disconnect-supervision-mame-device-implementation.spec.md), and [`audio-mame-device-architecture.md`](audio-mame-device-architecture.md).

## Port classes

| Class | Range | Notes |
| ----- | ----- | ----- |
| Z180 internal peripherals | per Z180 datasheet, base set by `ICR` (default `0x40`) | MMU, ASCI 0/1, PRT 0/1, INT controller, DMA, refresh, wait states. |
| External peripherals (known) | board-profile specific | VFD, keypad, card, smartcard, coin, modem control, NVRAM, alerter, security inputs. |
| External peripherals (suspected) | board-profile specific | Used by firmware but not yet attributed to a device; documented as `suspected` until evidence matures. |
| Unknown | rest | Logged on read/write per the unknown-port policy below. |

## Per-port table (template; populated per profile)

| Port | Direction | Active | Owning device | Firmware driver evidence | Emulator handler file | Test | Status |
| ---- | --------- | ------ | ------------- | ------------------------ | --------------------- | ---- | ------ |
| `0x00` | R/W | high | `<device>` | the firmware evidence inventory | `src/mame/coinline/millennium_<device>.cpp` | `tests/devices/test_<device>_*.cpp` | known/suspected/unknown |
| `0x40` | W | n/a | hw control (voice reset bit among others) | pinned in `fixtures/board/voiceware-command-map.json` | `millennium_voiceware.cpp` (planned) | `test-plans/voiceware-tests.md` | known (partial) |
| `0x42` | W | n/a | voice ROM bank select `[3:0]` | same | same | same | known |
| `0x61` | W | n/a | voice phrase write | same | same | same | known |
| ... | | | | | | | |

> Tranche E1 pins the first **known** rows (Z180 on-chip registers, PIO, VFD display port, and board status/control) in [`fixtures/board/io-port-map.json`](../fixtures/board/io-port-map.json) with evidence recorded under ``. Remaining ports are **unknown** until tranche E2 I/O traces promote them; the unknown-port logger still captures every access.

## Unknown-port policy

When the firmware reads from or writes to a port not declared in the I/O port map:

| On read | On write |
| ------- | -------- |
| Log the access (see schema below). | Log the access (see schema below). |
| Return a configurable default (`0xFF` or `0x00` per board profile). | Discard the value. |
| **Never** silently return a magic value that depends on the port number. | **Never** treat a write as configuration. |
| **Never** treat repeated reads as side-effecting unless the device spec says so. | |

Each access produces a structured log entry:

```jsonc
{
  "ts": "2026-05-03T11:00:00.012345Z",
  "cycle": 1234567890,
  "pc": "0x4321",
  "port": "0xC4",
  "rw": "r",
  "value": "0xFF",
  "source_symbol": null,
  "note": "unknown_port"
}
```

The `source_symbol` field is populated by the symbol/map loader (tranche E13) when a symbol is available. `note` is one of `unknown_port`, `suspected_port`, or `known_port`. Boot-time observations of unknown ports are aggregated in the firmware evidence inventory.

## Default values

| Default | When |
| ------- | ---- |
| `0xFF` | The bus is undriven (typical for unmapped ports on the Millennium board). |
| `0x00` | A specific board's pull-down configuration dictates it. |
| Device-specific | The port is owned by a device whose spec defines a defined return value. |

The default is a per-board-profile setting (`io_ports.unknown_default`).

## Z180 internal peripherals

The Z180 internal peripherals occupy a contiguous I/O range starting at the `ICR`-configured base (default `0x40`). The decoded set is enumerated in [`z180-internal-peripherals.md`](z180-internal-peripherals.md) and tested in `tests/devices/test_z180_*.cpp`.

The driver routes Z180 internal-peripheral I/O to MAME's Z180 device directly. Custom external-peripheral handlers occupy the rest of the I/O space.

## I/O-port JSON schema (informal)

Full schema in [`../specs/io-port-map.spec.md`](../specs/io-port-map.spec.md). Informally:

```jsonc
{
  "unknown_default": "0xFF",
  "ports": [
    {
      "port": "0xA0",
      "direction": "r",
      "active_level": "high",
      "device": "vfd",
      "evidence": "the firmware evidence inventory",
      "handler": "src/mame/coinline/millennium_vfd.cpp",
      "test": "tests/devices/test_vfd_command_decode.cpp",
      "status": "known"
    }
  ]
}
```

## Testing

- `tests/devices/test_unknown_port_logging.cpp` — verifies that an unknown port produces a structured log entry on read and write, with PC and cycle filled in.
- `tests/devices/test_io_port_defaults.cpp` — verifies that each port's default matches the active board profile.
- Per-device tests under `tests/devices/test_<device>_*.cpp` — exercise the known ports.

## Cross-references

- [`memory-map.md`](memory-map.md) — program space.
- [`z180-internal-peripherals.md`](z180-internal-peripherals.md) — Z180 ICR / internal peripherals.
- [`device-model.md`](device-model.md) — what each device must implement when it claims a port.
