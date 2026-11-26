# Run the emulator

Extract the entire Windows x64 ZIP to a writable folder and open **Start OEM.cmd** for the two-line terminal or **Start Graphical.cmd** for the 11-line terminal. Click the terminal window to focus the keys. Each launcher starts its own bundled central office. It can launch on Windows without a separate MAME, Python, or MSYS2 installation.

The two profiles may run concurrently. OEM uses `coinline.ini` and `data/oem/`;
graphical uses `coinline-graphical.ini` (copy the corresponding example) and
`data/graphical/`. Use separate terminal wire IDs, NCC table sets and SIP
accounts. The default SIP UDP ports are 5087 and 5088, and dynamic RTP ranges
are 10000-14999 and 15000-19999 respectively. If configuring fixed ports,
choose distinct values for the two profiles. An existing OEM installation
retains its original `data/oem/` path. A prior graphical installation launched
through OEM can be moved by the operator to the matching subdirectory under
`data/graphical/` with both instances closed; nothing is migrated automatically.

For a larger Windows window, use `"Start OEM.cmd" --scale 2` or
`"Start Graphical.cmd" --scale 2`. This requests a 720x1700 client bound,
which the native aspect ratio and monitor work area may constrain. Use
`--maximize` to fit the available monitor rather than use a fixed bound.
Set `[display] window_scale = 2.0` (and optionally `window_maximized = true`)
in the corresponding INI to retain the preference across launches.

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
state. The registered NLA1X08 and NLA1X09 images automatically select their
graphical profile. The full, corrected full candidate, and earlier focused
NLA1X09 variants use separate hash-bound battery states. The corrected
candidate passes CO and answer supervision from blank storage; the existing
packaged image remains the reference until the full INSTALL passes.
The graphical board profile and Multi-pay V1.3 control-board preset use a
24.576 MHz oscillator, corresponding to a 12.288 MHz Z180 processor clock.
The conventional earlier-board profile retains its 12.288 MHz oscillator.
This MTR 2.12-based graphical/ADSI terminal (type 40, DLA 1.3) completed
cold INSTALL, a separate in-service restart, and rendered downloaded
bitmap advertisements. Its original INSTALL chip-card check failed, but a
later native pin-level reader passed Craft item 728 twice and the standard
NLA1X05 complete INSTALL. A magnetic swipe obtained an approved simulated
authorization and connected a local call; no CDR or debit was observed.
Funded chip applications remain a separate gate. Free graphical menu
navigation is qualified with the compatible voice service.

For a local card bench, choose synthetic card fixtures with `--mag-card`
and `--smart-card` or set `mag_fixture` and `smart_fixture` under
`[cards]` in `coinline.ini`. Paths are relative to the extracted package
or absolute. The default chip-card fixture exercises the physical contacts;
it is not a funded payment card. The optional `smartcard-async-t0-lab.json`
and `smartcard-async-t1-lab.json` fixtures use the actual later-board ASCI0
pin path and a read-only synthetic application. They are intended for a
compatible transport test, not recognition by historical payment firmware.
Use `--local` for simulated calls. The explicit writable synchronous lab
fixture persists only its bounded one-way test memory in a separate smart-card
NVRAM device file; it does not modify the terminal's own battery data.

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
visual-prompt records, MT85 and MT86. Synchronous chip contact testing has
passed on the later Multi-pay board; funded chip-card applications and
several other firmware families still have open gates.
See the [batch acceptance guide](tools/acceptance/README.md)
and [firmware results catalog](docs/uat/firmware-acceptance-catalog.md).

An optional graphical voice-service route is available with `--adsi`. Set
the exact local aliases, target and UDP port under `[adsi]` in `coinline.ini`.
The corresponding terminal tables must permit the aliases. This route is
loopback only and has no external SIP fallback. See the
[CO bridge instructions](tools/line-jack/README.md#local-adsi-service).
Live firmware sent real A and D1 tones, decoded the service's FSK CONNECT
and welcome frame within the deadlines, and displayed the native welcome
menu. The service's voice-between-screens profile enabled genuine keypad
News/Back and softkey Exit, followed by clean release; see
[ADSI navigation qualification](docs/status/connected-dtmf-adsi-welcome-2026-11-25.md).

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

Connected keypad digits are separate from initial call routing. The `[sip]`
`dtmf_mode = auto` default uses negotiated RFC 4733 RTP telephone events,
then explicitly supported legacy SIP INFO, otherwise in-band G.711 tones.
Set `dtmf_mode = inband` or `info` only if the peer requires that alternative.
The exact local ADSI route always uses in-band tones.

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

The Windows launcher defaults to a compact OpenGL window with automatic frameskip and keeps the debugger and trace sidecars off. Use `--video gdi` if OpenGL is unavailable; the software renderer can skip additional display frames to preserve terminal timing. Direct3D is also selectable with `--video d3d` where supported. Mouse-driven title-bar moves and border resizes no longer pause the terminal; artwork redraws are briefly deferred until the drag ends. The standard Windows system menu retains its original modal behavior. The latest log directory contains `display.txt` or `graphic-vfd.json`, `emulator.log`, and `central-office.log`. Error 41 points to CO line detection. For error 24, verify the running NCC listener, matching wire ID, and published table assignment.

The [verification handoff](docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md) records the full MAME and CoinLine acceptance runs.
