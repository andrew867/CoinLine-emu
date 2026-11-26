# Batch firmware acceptance

The runner uses MAME's inputs to follow firmware prompts through INSTALL. Each attempt starts with its own empty battery and MAME configuration directories. A successful cold run is followed by a separate launch using the battery image written by the terminal. The batch uses the local central office and the CoinLine NCC listener on loopback.

## Run

Use a built `cl_millennium` executable, the generated telephony controller model, a local Voiceware pair for exploratory runs, and a provisioned lab terminal on CoinLine. The default input and output paths are under `build/firmware-acceptance/`. Prepare the firmware archive and read the candidate order in the [firmware acceptance catalog](../../docs/uat/firmware-acceptance-catalog.md).

The bundled [inventory](inventory.json) lists source links at pinned commits,
file roles, sizes, SHA-256 hashes, and the relative path for each archived
artifact. Obtain the local firmware and Voiceware inputs separately and place
them at the listed paths under the library root. A custom library may supply
its own `inventory.json`. Unavailable or mismatched files cannot be used in an
INSTALL run. The inventory contains metadata and digests only.

Put the lab settings in `build/firmware-acceptance/settings.json`. Keep this file with the ignored run inputs.

```json
{
  "line": "<10-digit lab terminal number>",
  "serial": "<10-digit lab terminal serial>",
  "pin": "<5-digit craft PIN>",
  "craft_access": "<7-digit synthetic Craft access code>",
  "ncc_port": 4000
}
```

For new synthetic table sets the provisioner defaults `craft_access` to the
factory keypad mnemonic if omitted. The seven-digit MT31 access field is
independent of the five-digit INSTALL PIN. Changing settings does not alter an
existing terminal's battery; a normal selected download or fresh INSTALL is
required to deliver new records.

Interactive graphical sessions also accept `--read-watch <local-file>`
for bounded, read-only program-bus timing diagnosis. Each line contains a
label, a physical hexadecimal address, and optionally a whitespace-separated
list of physical RAM byte addresses to snapshot. Watch output is restricted
to off-hook activity and at most 2,000 samples. The option is disabled by
default and never changes the observed data, CPU state, or installation.
Use `--read-watch-registers` to append the CPU register values at each
sample, and `--read-watch-per-frame 2` to limit each watch address to two
samples per video frame. The global limit is still enforced. This is
particularly useful when a frequently dispatched routine would otherwise
exhaust the diagnostic sample budget before another task can run.

For later boards with the internal contact-module expansion, the native
`smartcard_async_probe.py --protocol slots` test runs a synthetic CPU program
through all three module selectors and the customer contact. It checks
actual ASCI0 receive status and distinct read-only data. For real firmware
observation, `graphic_session.py --sam-pin-trace` records at most 4,096
changed selection/latch observations. The optional `--sam-slot0`,
`--sam-slot1` and `--sam-slot2` paths configure separate generic non-payment
async fixtures; absent paths leave the respective sockets unpopulated.
These tests do not provide or qualify historical payment authentication.

## Independent analogue hardware probes

Generate a single deterministic CAS/CONNECT line stimulus and run the installed
graphical terminal without a SIP or TCP audio sidecar:

```sh
python tools/acceptance/adsi_pcm_fixture.py build/firmware-acceptance/offline-adsi.pcm
python tools/acceptance/graphic_session.py --mame build/bin/coinline-mame.exe \
  --firmware <mapped-image> --battery <saved-battery-directory> --settings <lab-settings> \
  --adsi-offline-pcm build/firmware-acceptance/offline-adsi.pcm --quiet --sound
```

Keep a matching isolated NCC listener available for ordinary maintenance.
After the terminal returns to service, dial the configured local `free_call`.
The offline office answers only the actual matching outpulse and supplies
physical line polarity phases. A dedicated 8 kHz emulated timer then feeds
the analogue detector. No network response, decoded byte, terminal flag,
retry or fabricated ACK is injected. Compare the resulting native carrier,
UART and DTMF events to the live wire measurements; this is not itself SIP
menu qualification. Optional `--connect-pcm` to the fixture generator retains
the exact supplied local CONNECT waveform, including its captured silence.
For comparison, `--offline-co` retains the normal local CO control socket
but still supplies the receiver with deterministic PCM instead of network
audio. The service answer path and the sample-clock path can thereby be
measured independently.

The following synthetic CPU/bus probes require no installed payment or call
state and save their local evidence below the ignored acceptance output:

```sh
python tools/acceptance/enclosure_alarm_probe.py --mame build/bin/coinline-mame.exe
python tools/acceptance/data_jack_pin_probe.py --mame build/bin/coinline-mame.exe
python tools/acceptance/line_jack_probe.py --mame build/bin/coinline-mame.exe
```

The enclosure probe records actual emitted audio and verifies the warning
frequency against the GPIO edges. The data-jack probe checks only the later
equipment monitor and documented relay output combinations, not analogue
connection or a modem handshake. The line-jack probe checks initial native
loopback control signaling independently of the CO service.

## Batch execution

From `coinline-emu`, use Python 3.11 or newer in MSYS2 MINGW64 on Windows. On Linux or macOS, use the native Python and MAME build.

```sh
python tools/acceptance/run.py --inventory-only
python tools/acceptance/run.py --only NBA1F02
python tools/acceptance/run.py --skip NBA1F02
```

Omit `--only` and `--skip` to record the complete catalog. The batch runs sequentially against the selected lab terminal. Each distinct supported CPU image or unambiguous flash pair gets an attempt. A missing dump or unresolved layout gets a blocked row. Use `--mame`, `--library`, `--settings`, `--reference`, or `--controller` to select different input paths.

If the local CO receives no terminal hook state or Windows exits MAME before it can initialize, the runner records the infrastructure blocker and retries once from fresh state. Both attempts remain in the report.

For a later-board cold INSTALL, the runner starts with the housing open and
allows the telephony controller to settle. It follows a firmware startup
INSTALL prompt when offered. Other startup screens lead through a handset
cycle and Craft access. Warm runs start closed and verify the terminal's own
in-service display.

When a later firmware reports status 34 and offers a retry after its download,
the runner presses `*` once to continue the firmware's table update workflow
in the same terminal instance. Both attempts and their wire events are retained.

The INSTALL driver uses separate magnetic-stripe and smart-card inputs when the
terminal prompts for them. If the firmware allows an optional failed card check
to be continued, the result records that failure independently of the INSTALL
and restart result. `--uart-trace` captures accesses to the later-board coin
validator UART for diagnosis. The NCC frame report includes the MT44 request
reason byte as well as application table acknowledgements.
`--eeprom-trace` records native Microwire reads, writes and control commands.
The later calibration prompts allow the validator to settle, and permit one
repeat insertion if the display remains unchanged. The post-coin deadline
allows both timed escrow operations to finish.
`--read-watch` accepts a text file with a label and hexadecimal physical address
per line. It records the CPU's actual reads during NCC download and follow-up,
including their addresses, values and program counter.

Coin Basic firmware can start with a blank display. The bench models an attached
portable display after an initial detached preparation launch. The driver
enters Craft access and PIN on-hook, opens the cabinet, powers off, and
starts a second launch with the VFD attached and the firmware-written battery.
The driver closes the unlocked cabinet, repeats Craft access as needed, and
opens it again when prompted. It requires actual prompts before proceeding
through INSTALL. Use
`--voice-trace` to retain Voiceware commands during this diagnosis.
The terminal craft interface guide (Issue 00.01, Appendix C) describes separate
internal Coin Basic and external Inmate portable display modules on J7. The
maintenance procedure includes disconnecting power, attaching the display,
restoring power, and removing the display before regular operation.
With `--test-call`, a successful Coin Basic installation is followed by a
separate attached-display service restart and a detached-display call test.
This full sequence passed on the three available Coin Basic program images,
`NPE1S01`, `NAD4K02`, and `NAD4S02`. Inmate remains unqualified without
a usable program image.

Add `desk_key_card` to the settings with a path to a synthetic Desk craft card fixture if the lab profile uses a craft card. The runner inserts it when requested after Craft PIN entry. It uses the standard synthetic test card for the separate INSTALL reader check. Add `free_call` with the digits served by the local office and pass `--test-call` to check a locally answered call and hangup after restart.

For an isolated synthetic magnetic-card authorization bench, pass
`--test-mag-call` instead of `--test-call`. Provision the target's screening,
rate and test-card validation records first. The test presents the card at
the firmware prompt, dials the locally served destination, requires CO
answer supervision and checks return to service. The host-side authorization,
uploaded call record and synthetic settlement must be recorded independently;
the emulator result alone does not prove a debit. The synchronous contact
fixture is not a funded payment card.

For an interactive graphical voice-service bench, `graphic_session.py`
accepts `--adsi --sound`, the local route and normal terminal settings,
and a previous firmware-written `--battery` directory. Its command file
drives real panel inputs. `--keypad-trace` enables focused input and TP
transaction diagnostics without tracing every serial-clock edge. Neither
trace option is enabled in normal portable launches.

When firmware types need different NCC table sets, provision separate lab terminals and add a `firmware_overrides` object keyed by firmware ID. Each value can override the default terminal `line`, `serial`, `pin`, or other lab settings. This keeps the batch on the firmware-specific lab profiles without changing their assignments during a run.

For a later remote-download test, the lab settings also accept
`code_server_number` and integer `code_server_port`. They select the
optional local code-server route, separate from NCC, when the firmware
dials the matching alias. The complete upgrade protocol is outside the
current INSTALL runner.

### Provision local CoinLine

`provision.py` prepares lab firmware inventory entries, synthetic wire IDs, and
published INSTALL sets at the site of an existing lab terminal. Pass that
terminal's display name as `--anchor`. Omit `--apply` to list the planned
targets. The local API can use an operator token in
`COINLINE_LAB_OPERATOR_TOKEN` and an API key in `COINLINE_LAB_API_KEY` when
required. For a development instance, set `COINLINE_LAB_DEVELOPMENT_OPERATOR`
to the local operator audit name.

```sh
python tools/acceptance/provision.py --anchor "<lab terminal>" --apply
```

The tool reads example wire records from the staged public manager checkout.
It generates the Terminal Access and Installation Parameters records for each
lab ID and verifies the published record hashes before assigning the set.
The older recipe includes MT24 screening. The expanded recipe uses the
longer MT21 and MT92 layouts and MT134/135/136. The later expanded recipe
also supplies MT72, a 1,001-byte record including the message type. The V1.1
recipe adds the mandatory MT85 and MT86 visual-prompt records, 8,001 bytes
each. Desk/card recipes omit coin
validation and use the card terminal type in MT26. Coin Basic uses its own
terminal type and omits the card table. Set `desk_key_card` before creating
a Desk target to retain Craft access after installation. The host receives a dedicated
firmware version assignment even when its family catalogue lacks that release.
The local settings and provisioning report retain the per-firmware bindings.
Host publication and firmware acceptance are recorded independently.

## Board comparisons

The runner selects a hardware preset from the verified archive ID and records
the preset, motherboard I/O mapping, magnetic-reader DMA channel, profile hash, any program tag, and boot
checkpoints. Automatic mode tries an alternate earlier mapping after an
unsuccessful later-map boot. Explicit `--hardware` uses only the selected
preset's I/O mapping. The selected profile and the embedded tag are separate evidence.

Use `--board earlier` or `--board later` for a specific wiring map and `--profile` for a different board profile. A boot-only comparison uses fresh state for each mapping and stops before INSTALL. The optional hardware trace captures interrupt activity, address translation, and sampled Z180 registers, including the DMA status used for board comparisons.

Use `--telephony-trace` to inspect CSIO wire activity, qualified bytes, and
controller messages during the CO and answer-supervision checks. The
`--timeout` wall-clock limit defaults to 1200 seconds per launch, allowing
the larger V1.1 downloads and subsequent peripheral tests to finish even
when multiple builds share the machine. Firmware prompt deadlines remain
based on emulated time.

```sh
python tools/acceptance/run.py --only NKA1X02 --boot-only --board both --hardware-trace
python tools/acceptance/run.py --only NKA1X02 --boot-only --hardware multipay-v1.1 --io-trace
```

## Results

Each batch has a timestamped directory under `build/firmware-acceptance/runs/`.

| File | Evidence |
| --- | --- |
| `bin-hashes.json`, `SHA256SUMS` | Measured SHA-256 for every staged BIN, including Voiceware and other companion files. The JSON retains the expected digest, file role, and verification result. |
| `summary.md`, `batch.json` | Per-candidate status, board mapping, and a comparison of the initial Z180 register snapshots. The JSON includes the selected candidates, tool hashes, and observed gates. |
| `result.json` | Effective mapped-program hash, input hashes, cold and warm results, boot-register summary, NCC table ACKs, battery hashes, and persistent flash hashes for later boards. |
| `cold/`, `warm/` | VFD prompt and input history, terminal and CO logs, CRC-verified NCC frame summaries, and boot or hardware traces. |
| `portable-preflight/`, `detached/` | Coin Basic detached preparation and, with `--test-call`, detached-display call and hangup evidence. |

The run copies verified program bytes into an isolated emulator input directory. It registers the SHA-256 of the effective flash mapping there so the emulator can retain strict hash validation. The original controls and firmware write the install state through the modeled hardware.

An INSTALL pass requires the firmware's completion prompt followed by an in-service prompt. A restart pass requires a reported battery load and the returned service prompt. The individual keypad, card, and coin fields record the prompts actually reached and their observed completion. NCC ACKs establish transport acceptance, and a Voiceware trace establishes command activity. Audible output, table-specific behavior, and working hardware on a physical terminal require their own evidence.

For a later-board INSTALL pass, the runner also requires a persistent flash
state file and a reported load of that state during the warm launch. The synthetic
flash-bus test is independent of an NCC install:

```sh
python tools/acceptance/flash_probe.py --mame <path-to-mamecoinline>
```

It erases and programs a sector on each chip through CPU memory accesses,
checks that neighboring sectors retain their contents, verifies persistence
after a separate launch, and confirms the original input bytes stay unchanged.
See [remote firmware downloads](../../docs/firmware-download-storage.md) for
the end-to-end upgrade gates.

Run outputs can include lab identifiers, state files, and firmware images. The default output path is ignored by Git.

## Windows window and clock qualification

`window_drag_probe.py` exercises native Win32 frame capture, geometry, release
and cancellation while observing the emulated machine clock. It posts only
messages to its own test window and does not move the global mouse cursor.
The default synthetic CPU loop isolates the OSD path. An optional later-board
firmware image and `--sound` exercise the real machine with audio enabled.

```sh
python tools/acceptance/window_drag_probe.py --mame <path-to-mamecoinline.exe> --video opengl
python tools/acceptance/window_drag_probe.py --mame <path-to-mamecoinline.exe> --video opengl --free-aspect
python tools/acceptance/window_drag_probe.py --mame <path-to-mamecoinline.exe> --firmware <mapped-flash.bin> --sound
```

The free-aspect test checks actual resize geometry independently of the
monitor's fixed-aspect minimum/maximum dimensions. Results and machine-clock
samples remain in the ignored `build/firmware-acceptance/window-drag-probe/`
directory. The graphical acceptance session uses the same Windows renderer
defaults as the launcher. Set `--snapshot-interval 0` to avoid periodic PNG
encoding during wall-clock timing tests. `--external-co-port` binds the
emulator to a fixed local line jack and leaves the independently managed CO
and its lifecycle to the caller.

`window_scale_probe.py` checks the default, explicit 2x and unrestricted
maximized native window client geometry without changing any existing window.

```sh
python tools/acceptance/window_scale_probe.py --mame <path-to-mamecoinline.exe>
```

## Smart-card transport

The synthetic native program in `smartcard_async_probe.py` selects the real
later-board customer card socket, activates clock and reset, configures the
Z180 ASCI0 UART and checks the received ATR and the read-only lab application.
The T=0, inverse convention, T=1 block chaining and optional rate-selection
variants use actual pin-level serial traffic. `smartcard_pin_probe.py` also
checks synchronous contact programming, removal and a separate warm restart.

```sh
python tools/acceptance/smartcard_async_probe.py --mame <path-to-mamecoinline>
python tools/acceptance/smartcard_pin_probe.py --mame <path-to-mamecoinline>
```

Neither probe recognizes an issued purse, provides a payment authorization,
or modifies OEM firmware or terminal installation state.

For a genuine installed-firmware contact test, `smartcard_craft_probe.py`
uses a copy of the terminal battery, an isolated NCC port and the local CO,
then exercises the Craft menu, card insertion/removal and cabinet closure.

```sh
python tools/acceptance/smartcard_craft_probe.py --mame <native-binary> --firmware <mapped-flash.bin> --battery <installed-nvram-dir> --settings <local-lab-settings.json> --display 11line
```

Use `--display 2line` for the standard later-board display or `--package`
instead of `--firmware` for an extracted earlier OEM package. The default
menu number is 728 for the later board and 227 for the older card-reader
check; `--item` overrides it. Review the generated native screenshots and
pin trace before declaring a firmware contact-test pass.
`--runtime-contact` instead presents and removes the unfunded contact after
pickup, without dialing or entering Craft.
Use `--require-contact` to fail the probe if no physical reads occur or any
of the read bits differ from the bundled synthetic fixture.

## Connected keypad and SIP transport

For a focused cold OEM electrical check, `oem_answer_probe.py` runs the actual
packaged firmware with fresh memory, completes CO and answer supervision,
and stops at the NCC download prompt. It defaults to a loopback SIP registrar
without a public route, or accepts `--mode local` for the local office.

```sh
python tools/acceptance/oem_answer_probe.py --package <extracted-package-folder>
```

Use `--mame <path-to-mamecoinline>` to exercise a newly built binary using the
same package's ROM inputs.

`dual_profile_probe.py --package <extracted-package-folder>` copies the
package into an ignored test run, launches both terminal profiles and
verifies that their separate SIP accounts register through distinct local
ports against a loopback registrar. No external call is placed.

`connected_dtmf_native_probe.py` starts a real graphical terminal from a
copy of completed installation state and an isolated loopback SIP registrar
and RTP peer. It requires a permitted free-call destination in the lab
settings and waits for the SIP answer plus an emulated supervision interval
before pressing `25*#`. It checks the firmware-enabled native line events and
negotiated RFC 4733 packet timing. No public SIP account is used.

```sh
python tools/acceptance/connected_dtmf_native_probe.py --mame <path-to-mamecoinline> --firmware <mapped-flash.bin> --battery <installed-nvram-dir> --settings <lab-settings.json>
```

The script uses the CO's Python/PyVoIP dependencies, a working lab NCC for
any firmware maintenance calls and a graphical window. `--startup-frames`
and `--answer-frames` set the warm startup and answer allowances. Results are recorded under the
ignored `build/firmware-acceptance/connected-dtmf-sip-native/` tree.

`new_call_native_probe.py` takes the same four input paths and tests the
physical New Call key with an isolated loopback peer. It requires a first
answered dialog, a BYE after New Call, and a second answered dialog without
moving the hook switch. NCC is isolated; the original battery is not edited.
The run includes keypad and relay traces and before/after display snapshots.

`dialtone_native_probe.py` copies a completed installation into an isolated
run, captures the genuine early controller handshake and records native
handset audio before and after a physical first digit. It analyzes both
recorded channels for the nominal 350/440 Hz pair. The original battery is
never edited; the CO is local and the NCC port is isolated.

```sh
python tools/acceptance/dialtone_native_probe.py --package <extracted-package> --profile graphical --battery <installed-nvram-dir> --startup-frames 1200 --require-dialtone
```

Choose `--profile oem` for the standard two-line package, `--mame` to override
the packaged binary, and `--csio-trace` for startup serial diagnostics. The
WAV and screenshots stay in the ignored run folder.
