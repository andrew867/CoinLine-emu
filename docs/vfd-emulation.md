# VFD emulation

The terminal supports a conventional two-line character display and an optional
11-line graphical expansion. The native driver selects the display hardware
from the board profile. The character controller and graphical expansion have
separate models, interfaces and rendering paths.

## Graphical expansion

Select `--display 11line` with a compatible later-board firmware. The launcher
uses the 11-line board profile, graphical artwork view, and a separate terminal
state directory. The conventional view and default launch remain unchanged.
The launcher saves the four-page snapshot on clean exit. The current
text-prompt INSTALL runner is for the conventional display; graphical firmware
qualification initially uses the interactive view and graphical bus capture.

The expansion has four pages of 1,408 bytes. Each page uses 11 bands with a
128-byte column stride; the provisional visible raster is 126 × 88 dots.
The native driver preserves all eight bits of every column, separates the
write and display page selectors, and clocks the raw display RAM from
full 16-bit I/O addresses.

| I/O | Function |
| --- | --- |
| `0x80` | Display-control write and ready-status read |
| `0x90` | Three-group direct softkey scanner |
| `0xF0` | Reset release on zero write |
| `0x1280` through `0x17FF` | Even write-page pixel addresses |
| `0x1A80` through `0x1FFF` | Odd write-page pixel addresses |

The graphical renderer reads only the visible page, with no inserted text,
animation or softkey labels. Unselected pages retain their contents. The
six side buttons and four navigation buttons use the physical input mask
on the expansion scanner; no duplicate telephony-processor event is injected.
Two spare positions are available to diagnostic fixtures.

The ready bit is merged with the existing status inputs; the separate DSP
interface is not implemented by the display model. The graphical profile
exposes independent write and clear busy-cycle settings and a lamp-test
polarity setting. The defaults, zero busy cycles and active-high lamp
test, are provisional. Brightness uses a provisional linear intensity mapping.
Panel pitch and optical placement remain subject to bench verification.

See the [black-box hardware contract](../specs/hardware/11-line-vfd-black-box-handoff.md)
and [graphical acceptance probe](../tools/acceptance/graphic_vfd_probe.py).

## Character controller

Render text and graphic commands the firmware writes to the display. Capture the resulting buffer for evidence bundles.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Command port | Receives display commands (cursor, clear, custom characters). |
| Data port | Receives character bytes. |
| Status port | Returns busy / ready (and optionally cursor position). |

The conventional port routing comes from the selected motherboard wiring.

## State machine

```
idle -> command_in_progress -> idle
idle -> data_in_progress    -> idle
```

The conventional VFD parses character and command writes. The device
requirements and regression fixtures are in [the character display specification](../specs/vfd-device.spec.md).

## Timing behavior

- Each command consumes a configurable number of CPU cycles (`busy_cycles`).
- The status port reports `busy` until the command completes.

## Interrupts

The VFD does not normally raise interrupts in this terminal family.

## Variants

| Variant | Size | Notes |
| ------- | ---- | ----- |
| 2-line | 20 columns × 2 rows | Default; supports basic text + cursor control. |
| 11-line | 126 × 88 candidate raster, 128-byte stride | Raw column data, four hardware pages; no character decoding in the native driver. |

## Buffer snapshots

The character controller maintains a text buffer and a raw command stream.
Snapshots are exported as part of evidence bundles:

```jsonc
{
  "variant": "2line",
  "rows": 2,
  "columns": 20,
  "text": ["INSERT $1.00      ", "OR SWIPE CARD     "],
  "raw": "0x1B 0x40 ..."
}
```

For the graphical expansion, `COINLINE_GRAPHIC_VFD_TRACE` selects an optional
JSONL file of hardware cycles. `COINLINE_GRAPHIC_VFD_SNAPSHOT` selects an
exit-time JSON snapshot containing the four complete pages and control state.

## Fixtures

- `fixtures/display/vfd-2line-idle.json` — idle screen for 2-line variant.
- `fixtures/display/vfd-11line-ad.json` is a historical abstract character fixture, not a native graphical boot image.

## Tests

- `tests/devices/test_vfd_command_decode.cpp` — decode known commands.
- `tests/devices/test_vfd_buffer_snapshot.cpp` — snapshot equivalence.
- `tests/devices/test_vfd_2line_idle.cpp` — boot-to-idle pinned snapshot.
- `tests/devices/test_vfd_11line_ad.cpp` — advertising frame pinned snapshot.
- `tests/devices/test_graphic_vfd.cpp` tests page addresses, clear, brightness,
  ready masking, pixel orientation and the twelve raw key positions.

## Boot-milestone dependencies

- M5: first VFD I/O write observed.
- M6: VFD command decoded; buffer non-empty.
- M10: idle display matches reference.

## Acceptance criteria

- All declared commands are decoded.
- The character buffer matches its reference fixtures deterministically.
- The graphical probe validates native CPU writes, page snapshots, raw scan
  input and the generated raster. Actual graphical firmware boot and ADSI
  application operation remain separate acceptance gates.
- Status port returns `ready` after `busy_cycles` for every command.

## Cross-references

- [`../specs/vfd-device.spec.md`](../specs/vfd-device.spec.md).
- [`../test-plans/vfd-tests.md`](../test-plans/vfd-tests.md).
