# Run the emulator

Extract the entire Windows x64 ZIP to a writable folder and open **Start OEM.cmd**. Click the terminal window to focus the keys. The launcher starts its bundled central office. It can launch on Windows without a separate MAME, Python, or MSYS2 installation.

On Linux x64 or macOS on Apple Silicon, extract the archive, install the dependencies listed in **Quick Start.txt**, and run `./start-oem.sh`. These builds use the local central office and play terminal audio. SIP audio and capture are Windows features.

Open the included **Quick Start.txt** for setup. The [Windows operator guide](tools/windows/portable/quick-start.txt) includes the full INSTALL sequence for all platforms. Supply authorized ROMs using the [ROM input layout](BUILDING.md#include-your-local-rom-inputs).

## First INSTALL

Start CoinLine Server with its NCC listener on loopback TCP 4000. Use the lab terminal’s configured 10-digit wire ID as the line number and assign the table set for its firmware. The default MTR 1.9 Multi-Pay lab set includes the test card and free-call screening records. The decimal NCC number for `127.0.0.1` is `2130706433`. For a different local TCP port, copy `coinline.example.ini` to `coinline.ini` and set `ncc_port`.

For a future MTR 2.x remote code-server trial, the same configuration supports
a separate `code_server_number` and `code_server_port`. Leave both at their
defaults for ordinary operation. See the [remote download setup](docs/firmware-download-storage.md).

An out-of-service display on first launch is expected. Complete the firmware’s INSTALL with the CO and answer checks, NCC download, keypad, card, and coin checks. The portable launcher never writes the terminal tables or installation flags. When the terminal enters service, later runs reuse its firmware-written battery file.

## Hardware selection

The launcher defaults to `auto`. It selects a hardware preset from the verified
firmware inventory, then checks for a recognized program tag when the registry
entry has no firmware ID. The firmware hash registry supplies the legacy I/O
map when no preset is identified. Use `--hardware` to override selection.

The optional large graphical display is selected separately from the control
board revision with `--display 11line`, or `display = 11line` under
`[hardware]` in the launcher configuration. The default is `auto`, which
uses the verified firmware registry and otherwise selects `2line`.
Use a compatible later-board program image and its matching hardware preset.
The launcher selects the graphical artwork and isolates its persistent terminal
state. The registered NLA1X08 image automatically selects its graphical profile.
This MTR 2.12-based graphical/ADSI terminal (type 40, DLA 1.3) completed
cold INSTALL, a separate in-service restart, and rendered downloaded
bitmap advertisements. Its original INSTALL chip-card check failed, but a
later native pin-level reader passed Craft item 728 twice and the standard
NLA1X05 complete INSTALL. Funded-card transactions, graphical menus and
ADSI service interoperability remain separate acceptance gates.

For a local card bench, choose synthetic card fixtures with `--mag-card`
and `--smart-card` or set `mag_fixture` and `smart_fixture` under
`[cards]` in `coinline.ini`. Paths are relative to the extracted package
or absolute. The default chip-card fixture exercises the physical contacts;
it is not a funded payment card. Use `--local` for simulated calls.

| Preset | Typical firmware | I/O mapping | Magnetic reader DMA |
| --- | --- | --- | --- |
| `multipay-v1` | NBA1F02, NPA1S01, NPA1F01 | Earlier | 0 |
| `multipay-v1-dual-flash` | NCA1X03 | Later | 0 |
| `multipay-v1.1` | NKA1X02, NKA1X05 | Later | 1 |
| `multipay-v1.3` | NLA1X05, NOA1X04, NOK1X03, NQA1X01 | Later | 1 |
| `desk-rel1`, `desk-rel2` | NBJ1S03, NAJ2S05 | Earlier | 0 |
| `card-only`, `coin-basic` | M06CBBX1, NPE1S01 | Earlier | 0 where fitted |

These presets select the I/O and card clock wiring for each board. The earlier
board connects the coin validator to ASCI0. The later board uses a 600-baud serial
link through the external UART and IRQ1. Desk REL 1 and REL 2 have successful
INSTALL and restart cases. All four staged later V1.3 images (`NLA1X05`,
`NQA1X01`, `NOK1X03`, and `NOA1X04`) also completed INSTALL and returned to
service after restart. Both V1.1 images (`NKA1X02` and `NKA1X05`) also
passed. Their dedicated NCC set includes the two mandatory expanded
visual-prompt records, MT85 and MT86. Chip-card acceptance and several
other firmware families still have open gates.
See the [batch acceptance guide](tools/acceptance/README.md)
and [firmware results catalog](docs/uat/firmware-acceptance-catalog.md).

Coin Basic uses the earlier I/O map with display and Voiceware ports exchanged.
Use `--portable-display attached` during Craft setup, then exit and start the
same terminal with `--portable-display detached` to remove the temporary VFD.
The same setting is available under `[hardware]` in `coinline.ini`. The
terminal retains its own installed battery state.

Later-board sessions also retain their programmed flash contents in the
terminal's isolated state directory. The original program inputs remain
required to identify the initial board and its state. See the
[remote upgrade acceptance plan](docs/firmware-download-storage.md).

```text
"Start OEM.cmd" --local --hardware multipay-v1.1 --firmware firmware/U2.bin --flash1 firmware/X1.bin --rompath roms-v2 --check
```

The same arguments work with `./start-oem.sh` on Linux and macOS. `--rompath`
contains a `cl_millennium/` folder with the matching Voiceware pair.
Use `--check` to verify the input paths, mapped image SHA-256, and selected
profile. Remove `--check` to launch. OEM 1.9 retains its portable state
directory. Other mapped programs and presets each get their own battery and
configuration directories under `data/oem/`.

Direct MAME invocations can set `COINLINE_CARD_DMA_CHANNEL` to `0` or `1`.
The portable launcher and acceptance runner select it from the hardware preset.

## Controls

| Input | Action |
| --- | --- |
| H, the handset photo, or the handset button | Lift or replace the handset |
| 0–9 | Digits |
| Shift+8 or numpad `*` | Star |
| Shift+3 or numpad `/` | Pound |
| =, -, L, N | Volume up/down, language, next call |
| K, C, A, or the matching LED row | Toggle the lock, housing, or cash vault |
| P or click the card reader | Insert, read, and remove the synthetic test card |
| `[` | Insert or remove the synthetic smart card when prompted |
| V, B, M | Nickel, dime, quarter |
| Q W E R T | Quick-access positions 1 through 5, also clickable on the panel |
| Y G J X Z | Positions 6 through 10 on a ten-button profile |

The handset starts on-hook at each launch. The button reads **Lift handset** on-hook and **Hang up** off-hook. MAME keeps its function-key shortcuts. If an older profile has custom keyboard assignments, reset them in MAME's input menu or transfer its `nvram` folder into a fresh package and use the new defaults.

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
