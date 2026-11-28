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

On Linux x64 or macOS on Apple Silicon, extract the archive, install the dependencies listed in **Quick Start.txt**, and run `./start-oem.sh` or `./start-graphical.sh`. For SIP or the dedicated InfoLine routes, run `./setup-sip.sh` once to install the isolated Python runtime dependencies. POSIX builds use MAME for speaker playback and PortAudio for microphone capture. The usual local CO mode does not require the SIP runtime.

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
full-feature image with SHA-256 prefix `996a8ac5e81d` passed blank-storage
INSTALL, all 18 configuration records, generic customer contact, coin
tests, distinct internal module diagnostics, clock synchronization and a
separate in-service restart. An existing installation belonging
to another firmware digest is not reused. See [full-image qualification](docs/status/proton-graphical-qualification-2026-11-25.md)
for the exact digest and remaining application-level gates.
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
or absolute. The on-screen wallet shows the current magnetic and contact test
card. Click its name (or press F for magnetic, D for contact) to cycle through
the available cards. Press P or click Swipe for a complete magnetic swipe;
press `[` to insert the selected contact card and `]` to remove it (or click
the contact action button). The reader artwork follows the actual sensor
states. Selection is locked while either card occupies the reader.

Additional fixture paths can be configured as `mag_fixture_2` through
`mag_fixture_8` and `smart_fixture_2` through `smart_fixture_8`, in consecutive
slots. The default second magnetic card is a deliberately invalid checksum
test, not another funded account. The default second contact card switches
between the generic synchronous and asynchronous contact fixtures. The default
contact wallet also provides read-only synchronous LAB snapshots in slots 3,
4 and 5 (0.50, 5.00 and zero LAB units). These are invented transport fixtures,
not historical GPM103 payment cards; a terminal's historical application may
reject them. Optional asynchronous versions and the neutral memory-address test
are included as configurable fixture paths. Any explicitly configured extra
contact slot takes precedence over the automatic LAB catalogue. Set the second
slot to an empty value to disable that catalogue. Additional contact
slots are read-only; any explicitly writable bench fixture must occupy the
primary slot, whose existing persistent memory is preserved across selection.
These controls do not change terminal tables or payment authorization.

For a standalone GPM103-family synchronous EEPROM contact bench, select
`fixtures/cards/smartcard-sync-gpm103-contact.json` as the primary
`smart_fixture` in the profile's `[cards]` section. The fixture contains
16 bytes of arbitrary test data and two explicitly writable bench bits;
the nominal programming wait is 10 ms. The fixture geometry is deliberately
bounded, not a statement about the exact capacity of an original chip.
Use it to test reset, MSB-first reads, programming timing and persistence.
The writable image is kept in the terminal's local card-state storage.

The default chip-card fixture exercises the physical contacts;
the registered full graphical image `996a8ac5e81d` defaults to the qualified
`smartcard-async-contact-atr-lab.json` fixture instead of the synchronous
default. Neither is a funded payment card. The optional `smartcard-async-t0-lab.json`
and `smartcard-async-t1-lab.json` fixtures use the actual later-board ASCI0
pin path and a read-only synthetic application. They are intended for a
compatible transport test, not recognition by historical payment firmware.
Later-board profiles also accept independent internal module fixtures under
`[cards]` as `sam_slot0`, `sam_slot1`, and `sam_slot2`, or through the respective
`--sam-slot0`, `--sam-slot1` and `--sam-slot2` arguments. Leave them blank for
unpopulated contact positions. The included `smartcard-async-module-lab.json`
is a generic read-only contact test, not a payment authorization module.
The package also includes `app/lab_purse.py` for the independent LAB ledger
and its read-only snapshot export. See [lab purse](docs/lab-purse.md).
Use `--local` for simulated calls. The explicit writable synchronous lab
fixture persists only its bounded one-way test memory in a separate smart-card
NVRAM device file; it does not modify the terminal's own battery data.

The optional later-board data-jack monitor/relay profile can be selected
with `--data-jack downloaded` (or `desktop` / `universal`) and the matching
`data_jack_profile` in `[hardware]`. Leave it disabled for ordinary use.
This enables the independent equipment and loop inputs in MAME; it does
not yet provide a transparent external modem PCM/data connection.

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

The validator's physical coin windows and the terminal's downloaded coin
table must agree. Set `coin_validator` under `[hardware]` (or use
`--coin-validator`) independently of the motherboard and display selection.
`board-default` preserves the configured board fixture; `three-slot` is
the nickel/dime/quarter calibration, and `regional-nine` models the nine
active windows in the qualified graphical lab coin table. The graphical
example INI selects `regional-nine`. Existing INIs are not rewritten.
Choose the calibration corresponding to the actual downloaded coin table;
the emulator does not infer it from SRAM or alter the terminal's credits.

Optional InfoLine voice-service routes are available with `--adsi`. Set
the private IPv4 host and UDP port in `[adsi]` and the exact extension-to-alias
mapping in `[adsi.routes]`. A shared `adsi-routes.ini` (copy the example) can
configure both portable profiles without altering their normal SIP accounts.
Legacy single-target settings remain supported. The corresponding terminal
tables must permit the aliases, preferably with exact no-payment rules.
Direct routes support loopback or a private LAN endpoint and have no
external SIP fallback. See the
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

For a compatible SIP carrier, fill in the SIP registrar, username, and password in the selected profile's INI. On Windows open **Start OEM.cmd** or **Start Graphical.cmd**. On Linux/macOS run `./setup-sip.sh` if needed, then `./start-oem.sh --sip` or `./start-graphical.sh --sip`. The launcher reports **SIP calling** and calls the number the terminal outpulses. Dial a 10-digit North American number with an optional leading 1. For international dialing, use 011 followed by the country code and number. Leave `local_address` blank for automatic IPv4 selection. Launch with `--local` to use the local office even with SIP credentials present. The `outbound_digits` setting controls the local office test number.

Connected keypad digits are separate from initial call routing. The `[sip]`
`dtmf_mode = auto` default uses negotiated RFC 4733 RTP telephone events,
then explicitly supported legacy SIP INFO, otherwise in-band G.711 tones.
Set `dtmf_mode = inband` or `info` only if the peer requires that alternative.
The exact local ADSI route always uses in-band tones.

The handset uses the default host recording device: WinMM on Windows and PortAudio on Linux/macOS. Set `input_device` under `[audio]` in the selected profile's INI to a unique prefix of a recording device name. A missing or ambiguous device leaves the microphone silent and is reported in the emulator log. Set `capture_enabled = false` to leave the microphone silent without opening a host input. Restart after changing these settings. On Linux install `libportaudio2` and ensure the desktop audio session permits capture; on macOS install `portaudio` and allow microphone access for the launching app or terminal in System Settings. The host microphone is not required for local installation or terminal-generated voice, tones, or ADSI replies.

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

The Windows launcher defaults to a compact OpenGL window with automatic frameskip up to 10 and keeps the debugger and trace sidecars off. This prioritizes the terminal and audio clock over artwork under load. Use `--video gdi` if OpenGL is unavailable; Direct3D is also selectable with `--video d3d` where supported. Mouse-driven title-bar moves and border resizes no longer pause the terminal; artwork redraws are briefly deferred until the drag ends. The standard Windows system menu retains its original modal behavior. The host speaker buffer target defaults to 240 ms, configurable as `[audio] playback_buffer_ms` or `--audio-buffer-ms 40..500`. Windows uses above-normal process priority, configurable as `[performance] host_priority` or `--host-priority normal`. Set `[display] max_frameskip` or `--max-frameskip 0..10` to change the automatic redraw limit. Zero selects MAME's default automatic limit of 10; legacy values of 11 are normalized to 10 without rewriting the INI. These host settings do not alter the emulated hardware clock or remote media deadlines. The latest log directory contains `display.txt` or `graphic-vfd.json`, `emulator.log`, and `central-office.log`. Error 41 points to CO line detection. For error 24, verify the running NCC listener, matching wire ID, and published table assignment.

The [verification handoff](docs/status/ncc-install-firmware-stx-handoff-2026-11-23.md) records the full MAME and CoinLine acceptance runs.
