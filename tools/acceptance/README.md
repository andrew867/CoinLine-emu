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
  "ncc_port": 4000
}
```

From repository root, use Python 3.11 or newer in MSYS2 MINGW64 on Windows. On Linux or macOS, use the native Python and MAME build.

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

When firmware types need different NCC table sets, provision separate lab terminals and add a `firmware_overrides` object keyed by firmware ID. Each value can override the default terminal `line`, `serial`, `pin`, or other lab settings. This keeps the batch on the firmware-specific lab profiles without changing their assignments during a run.

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

It programs both chips through CPU memory accesses, checks persistence after
a separate launch, and verifies that the original input bytes stay unchanged.
See [remote firmware downloads](../../docs/firmware-download-storage.md) for
the end-to-end upgrade gates.

Run outputs can include lab identifiers, state files, and firmware images. The default output path is ignored by Git.
