# Board profiles

A board profile is a JSON document that selects per-revision configuration: VFD variant, keypad layout, card-reader presence, coin-validator configuration, modem ports, NVRAM size, and table-storage size. The board profile is supplied to the emulator at launch via `-board` (MAME) or `--board` (clean-room). Profiles live under `fixtures/board/`.

## Why board profiles exist

The Millennium-compatible terminal family has multiple revisions and OEM SKUs. Different revisions differ in display variant (2-line vs 11-line VFD), keypad and quick-access key complement, card-reader presence, coin-validator type, and storage layout. Hard-coding any of those choices into the driver would force a separate driver per SKU. A board profile factor of variation keeps a single driver and lets fixtures express SKU difference.

## Reference profiles

| Profile | Display | Use |
| ------- | ------- | --- |
| `board-profile-2line-vfd.json` | 2-line VFD | Baseline. Used for early bring-up and the boot-to-idle scenario. |
| `board-profile-11line-vfd.json` | 11-line VFD | Used for advertising-screen scenarios and full-feature exercises. |

Operators may add SKU-specific profiles by copying one of the references and editing the JSON.

## Board profile schema (informal)

The formal schema is in [`../specs/z180-board-profile.spec.md`](../specs/z180-board-profile.spec.md). Informally:

```jsonc
{
  "profile_id": "millennium-2line-vfd",
  "display": {
    "type": "vfd",
    "variant": "2line",
    "columns": 20,
    "rows": 2
  },
  "keypad": {
    "layout": "3x4",
    "quick_access_keys": [...],
    "volume_up": "vol_up",
    "volume_down": "vol_down",
    "language": "lang"
  },
  "card_reader": { "present": true, "type": "magnetic" },
  "smartcard": { "present": true, "type": "iso7816-memory" },
  "coin": { "validator_type": "pulse", "denominations": [5, 10, 25, 100] },
  "alerter": { "present": true },
  "security": {
    "lock": true,
    "door": true,
    "vault": true,
    "service_switch": true
  },
  "memory": {
    "rom_size": 524288,
    "ram_size": 32768,
    "nvram_base": "0x...",
    "nvram_size": 8192,
    "table_storage_base": "0x...",
    "table_storage_size": 32768,
    "dla_stage_base": "0x...",
    "dla_stage_size": 65536
  },
  "z180": {
    "clock_hz": 12288000,
    "wait_states": { "rom": 1, "ram": 0, "io": 1 }
  },
  "modem": {
    "asci_channel": 0,
    "default_baud": 1200,
    "default_dcd": false,
    "default_cts": true
  },
  "host_bridge": {
    "transport": "tcp",
    "default_endpoint": "tcp://127.0.0.1:5210"
  },
  "io_ports": {
    "on_unknown": "log_and_continue"
  },
  "artwork": {
    "front_image": "artwork/millennium-terminal-front.png",
    "layout": "artwork/millennium.lay"
  }
}
```

## How profiles are validated

- Schema validation in `tests/fixtures/test_board_profile_schema.py`.
- Cross-fixture consistency in CI: NVRAM size in the profile must match `fixtures/nvram/factory-default.nvram.json`'s declared size.
- Memory map alignment: `nvram_base + nvram_size` must not overlap `table_storage_base..+table_storage_size`.

## Selecting a profile at runtime

```
./coinline-emu -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json
```

If `-board` is omitted, the driver uses `board-profile-2line-vfd.json` as the default.

## Profile authoring guide

When adding a profile for a new SKU:

1. Copy `board-profile-2line-vfd.json` to `board-profile-<sku>.json`.
2. Update only the fields that differ.
3. Add a row to the reference table above.
4. Add a scenario or two that targets the SKU under `fixtures/scenarios/`.
5. Update the schema test to keep it strict.

## Cross-references

- [`memory-map.md`](memory-map.md) — region addresses.
- [`io-port-map.md`](io-port-map.md) — port addresses.
- [`z180-core.md`](z180-core.md) — clock and wait-state model.
- [`vfd-emulation.md`](vfd-emulation.md) — VFD variants.
- [`artwork-and-layout.md`](artwork-and-layout.md) — front-panel image.
