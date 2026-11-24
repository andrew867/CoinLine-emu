# Run the emulator

Extract the entire Windows x64 ZIP to a writable folder and open **Start OEM.cmd**. Click the terminal window to focus the keys. The launcher starts its bundled central office. It can launch on Windows without a separate MAME, Python, or MSYS2 installation.

On Linux x64 or macOS on Apple Silicon, extract the archive, install the dependencies listed in **Quick Start.txt**, and run `./start-oem.sh`. These builds use the local central office and play terminal audio. SIP audio and capture are Windows features.

Open the included **Quick Start.txt** for setup. The [Windows operator guide](tools/windows/portable/quick-start.txt) includes the full INSTALL sequence for all platforms. Supply authorized ROMs using the [ROM input layout](BUILDING.md#include-your-local-rom-inputs).

## First INSTALL

Start CoinLine Server with its NCC listener on loopback TCP 4000. Use the lab terminal’s configured 10-digit wire ID as the line number and assign the validated ten-record lab set, including the test card and free-call screening records. The decimal NCC number for `127.0.0.1` is `2130706433`. For a different local TCP port, copy `coinline.example.ini` to `coinline.ini` and set `ncc_port`.

An out-of-service display on first launch is expected. Complete the firmware’s INSTALL with the CO and answer checks, NCC download, keypad, card, and coin checks. The portable launcher never writes the terminal tables or installation flags. When the terminal enters service, later runs reuse its firmware-written battery file.

## Controls

| Input | Action |
| --- | --- |
| H | Lift or replace the handset |
| 0–9 | Digits |
| Shift+8 or numpad `*` | Star |
| Shift+3 or numpad `/` | Pound |
| =, -, L, N | Volume up/down, language, next call |
| K, C, A, or the matching LED row | Toggle the lock, housing, or cash vault |
| P or click the card reader | Insert, read, and remove the synthetic test card |
| V, B, M | Nickel, dime, quarter |
| Q W E R T | Quick-access positions 1 through 5, also clickable on the panel |
| Y G J X Z | Positions 6 through 10 on a ten-button profile |

MAME keeps its function-key shortcuts. If an older profile has custom keyboard assignments, reset them in MAME's input menu or transfer its `nvram` folder into a fresh package and use the new defaults.

## Calls and audio

The volume buttons step through four handset receive levels. The VFD brightness menu offers 50%, 75%, and 100%.

With blank SIP settings, the local central office plays dial tone and answers the configured outbound digits, which default to `18005551212`. With the lab free-call policy, the demo places this call without a rating request. The OEM can ask CoinLine for a live quote on a paid lab destination. Its spoken cents use five-cent increments.

For SignalWire, fill in the SIP registrar, username, and password in `coinline.ini` and open **Start OEM.cmd**. The launcher reports **SIP calling** and calls the number the terminal outpulses. Dial a 10-digit North American number with an optional leading 1. For international dialing, use 011 followed by the country code and number. Leave `local_address` blank for automatic IPv4 selection. Launch with `--local` to use the local office even with SIP credentials present. The `outbound_digits` setting controls the local office test number.

The handset uses the default Windows recording device. Set `input_device` under `[audio]` in `coinline.ini` to a unique prefix of a Windows recording device name to choose a microphone or virtual mixer output for the emulator. Restart the emulator after changing the input.

```cmd
"Start OEM.cmd" --sip
```

The launcher stores `cfg`, `nvram`, and `logs` under `data/oem`. Close the emulator normally to complete the save. To carry an installed terminal forward, copy `data/oem/nvram` to the same location in the new package while both copies are closed.

## Diagnostics

```cmd
"Start OEM.cmd" --check
"Start OEM.cmd" --headless --seconds 18
"Start OEM.cmd" --record
```

The launcher defaults to a compact GDI window with automatic frameskip and keeps the debugger and trace sidecars off. Use `--video d3d` or `--video opengl` to test those renderers on suitable displays. The latest log directory contains `display.txt`, `emulator.log`, and `central-office.log`. Error 41 points to CO line detection. For error 24, verify the running NCC listener, matching wire ID, and published table assignment.

The [verification handoff](docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md) records the full MAME and CoinLine acceptance runs.
