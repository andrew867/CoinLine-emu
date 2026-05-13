# MAME driver plan


## Driver identity

| Field | Value |
| ----- | ----- |
| MAME machine short-name | `millennium` |
| Display name | "Millennium-compatible Z180 payphone terminal" |
| Source root | `src/mame/coinline/` |
| Driver entry file | `millennium.cpp` |
| Header | `millennium.h` |
| Build subtarget | `coinline` |

## Source files

| File | Purpose |
| ---- | ------- |
| `millennium.cpp` | Driver entry point: machine config, address map binding, input ports, layout, ROM region |
| `millennium.h` | Driver class header |
| `millennium_state.cpp` | Machine state container; reset; clock plumbing |
| `millennium_state.h` | State class header |
| `millennium_memory.cpp` | Address map decoding |
| `millennium_memory.h` | Memory header |
| `millennium_io.cpp` | I/O map decoding + unknown-port logger |
| `millennium_io.h` | I/O header |
| `millennium_vfd.cpp/h` | VFD device |
| `millennium_keypad.cpp/h` | Keypad + hookswitch + quick-access keys |
| `millennium_card.cpp/h` | Magnetic card reader |
| `millennium_smartcard.cpp/h` | Smart-card / memory-card |
| `millennium_coin.cpp/h` | Coin validator |
| `millennium_modem.cpp/h` | Modem UART glue around the Z180 ASCI |
| `millennium_nvram.cpp/h` | NVRAM + table storage |
| `millennium_audio.cpp/h` | Alerter + handset audio |
| `millennium_security.cpp/h` | Lock / door / vault / service |
| `millennium_hostbridge.cpp/h` | Host bridge transport |
| `millennium_debug.cpp/h` | Trace, symbol, and map loader hooks |

## ROM region

```
ROM_START(millennium)
    ROM_REGION(0x80000, "maincpu", 0)
    ROM_LOAD("flash.bin", 0x000000, 0x100000, CRC(...) SHA1(...))
ROM_END
```

The ROM region size is finalized once `../firmware/flash.bin` is known. The CRC and SHA1 in the `ROM_LOAD` macro come from the operator's firmware binary; both values are also recorded in `fixtures/firmware/firmware-hashes.json`.

> The ROM file itself is not committed to this repository. Operators place the file under `roms/` per [`../RUNNING.md`](../RUNNING.md).

## RAM and NVRAM regions

| Region | Bus | Size | Source | Notes |
| ------ | --- | ---- | ------ | ----- |
| `mainram` | program | per board profile | dynamic | Cleared at reset by firmware. |
| `nvram` | program | per board profile | `fixtures/nvram/factory-default.nvram.json` | Battery-backed in hardware; persisted by MAME's NVRAM machinery. |
| `tablestore` | program | per board profile | initially empty | Receives table-distribution writes; verified by checksum. |
| `dlastage` | program | per board profile | initially empty | Firmware-download staging region. |

Region sizes and base addresses come from the active board profile (`fixtures/board/board-profile-*.json`).

## CPU device

```
config.set_default_device("maincpu", Z180);
config.set_clock("maincpu", XTAL(...).hz());
config.set_addrmap(AS_PROGRAM, &millennium_state::memory_map);
config.set_addrmap(AS_IO,      &millennium_state::io_map);
```

The exact crystal frequency is part of the board profile and pinned in [`z180-core.md`](z180-core.md).

## Address map (program space)

The full map lives in [`memory-map.md`](memory-map.md). The driver wires ROM, RAM, NVRAM, table storage, and firmware-download staging to the addresses specified there.

## I/O map

The full map lives in [`io-port-map.md`](io-port-map.md). The driver routes known ports to per-device handlers and unknown ports to the logger.

## Input ports

The MAME `INPUT_PORTS_START(millennium)` block declares:

- Numeric keypad (0–9, *, #).
- Quick-access keys per the board profile.
- Volume up / down.
- Language toggle.
- Hookswitch (lift / hangup).
- Card slot insertion (magnetic + smart card).
- Coin input + coin return + coin-jam fault button (test-only).
- Lock + door + vault + service-mode switches.
- Reset.

Each input is wired to the keypad/security/hookswitch device per [`keypad-emulation.md`](keypad-emulation.md), [`hookswitch-and-handset.md`](hookswitch-and-handset.md), [`coin-validator-emulation.md`](coin-validator-emulation.md), [`card-reader-emulation.md`](card-reader-emulation.md), [`smartcard-emulation.md`](smartcard-emulation.md), and [`lock-door-vault-service.md`](lock-door-vault-service.md).

## Artwork layout

The driver registers `artwork/millennium.lay`. The `.lay` file overlays the front-panel image at `artwork/millennium-terminal-front.png` (sourced from `docs/images/terminal-unbranded.webp`) with VFD regions and clickable input regions per [`artwork-and-layout.md`](artwork-and-layout.md).

## Device children

| Tag | Type | Source |
| --- | ---- | ------ |
| `vfd` | `millennium_vfd_device` | `millennium_vfd.cpp` |
| `keypad` | `millennium_keypad_device` | `millennium_keypad.cpp` |
| `card` | `millennium_card_device` | `millennium_card.cpp` |
| `smartcard` | `millennium_smartcard_device` | `millennium_smartcard.cpp` |
| `coin` | `millennium_coin_device` | `millennium_coin.cpp` |
| `modem` | `millennium_modem_device` | `millennium_modem.cpp` |
| `nvram` | `millennium_nvram_device` | `millennium_nvram.cpp` |
| `audio` | `millennium_audio_device` | `millennium_audio.cpp` |
| `security` | `millennium_security_device` | `millennium_security.cpp` |
| `hostbridge` | `millennium_hostbridge_device` | `millennium_hostbridge.cpp` |
| `debug` | `millennium_debug_device` | `millennium_debug.cpp` |

## Command-line examples

```
# Boot to idle with default board profile
./mame64 millennium -firmware ../firmware/flash.bin \
    -cfg_directory cfg -nvram_directory nvram \
    -board fixtures/board/board-profile-2line-vfd.json

# Run a scenario and emit an evidence bundle
./mame64 millennium -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -scenario fixtures/scenarios/boot-to-idle.json \
    -evidence out/boot-to-idle/

# Headless CI run
./mame64 millennium -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -scenario fixtures/scenarios/boot-to-idle.json \
    -nowindow -nosound -evidence out/
```

## Debug-mode examples

```
# Open MAME debugger at boot
./mame64 millennium -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -debug

# In the debugger:
#   bp 0x0000           # break at reset vector
#   wpset 0x8000,1,r    # watch first NVRAM byte read
#   trace run.trace     # turn on tracing
```

## Driver registration

The driver is registered in MAME's driver list as:

```
GAME(year, millennium, 0, millennium, millennium, millennium_state, init_millennium, ROT0, "<vendor>", "Millennium-compatible Z180 payphone terminal", MACHINE_NOT_WORKING|MACHINE_NO_SOUND)
```

The `MACHINE_NOT_WORKING` flag is removed once acceptance gates A–H pass per [`acceptance-test-plan.md`](acceptance-test-plan.md). `MACHINE_NO_SOUND` is removed when alerter audio lands in tranche E9.

## Cross-references

- [`memory-map.md`](memory-map.md), [`io-port-map.md`](io-port-map.md), [`interrupt-map.md`](interrupt-map.md) — what the driver wires.
- [`z180-core.md`](z180-core.md), [`z180-internal-peripherals.md`](z180-internal-peripherals.md) — Z180 specifics.
- [`device-model.md`](device-model.md) — common device contract.
- [`artwork-and-layout.md`](artwork-and-layout.md) — `.lay` file.
- [`scenario-runner.md`](scenario-runner.md), [`evidence-bundles.md`](evidence-bundles.md) — `-scenario` and `-evidence` flags.
