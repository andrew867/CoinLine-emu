# Audio routing emulation (telephony processor protocol)

## Purpose

Model **microphone (TX)** and **earpiece (RX)** gates and sidetone staging as explicit state updated by **single-byte commands**. Do not infer analog mixing — only **effective gates** toward modeled sinks.

## Modeled gate variables (minimum)

| Variable | Meaning |
| -------- | ------- |
| `gate_tx` | Microphone audio toward CO (`TX_UNMUTE`=`open`, `TX_MUTE`=`closed`) |
| `gate_rx` | CO/voice mix toward earpiece (`RX_UNMUTE`=`open`, `RX_MUTE`=`closed`) |
| `sidetone_mode` | `on` / `off` per `SIDETONE_SUPPRESSION_*` |
| `dtmf_from_pad` | `enabled` / `disabled` per `DIAL_PAD_DTMF_*` |
| `rx_gain_step` | Integer stage from volume opcodes |

## Host → processor commands

| Command | Hex | Gate effect |
| ------- | --- | ----------- |
| `TX_UNMUTE` / `TX_MUTE` | `0x26` / `0x27` | `gate_tx` |
| `RX_UNMUTE` / `RX_MUTE` | `0x28` / `0x29` | `gate_rx` |
| `SIDETONE_SUPPRESSION_OFF` / `ON` | `0x2A` / `0x2B` | `sidetone_mode` |
| `DIAL_PAD_DTMF_ENABLE` / `DISABLE` | `0x2E` / `0x2D` | `dtmf_from_pad` |
| `PRESET_VOLUME_LEVEL_*` | `0x20`–`0x23` | snap `rx_gain_step` |
| `VOLUME_LEVEL_UP` / `DOWN` | `0x25` / `0x24` | increment/decrement `rx_gain_step` |
| `CALL_STATE_*` | `0x40`–`0x46` | supervision/billing class shadow |

Full table: [`../fixtures/board/audio-routing-state-map.json`](../fixtures/board/audio-routing-state-map.json).

## Line vs handset (modeled)

| Source | Delivered when |
| ------ | -------------- |
| CO receive model | `gate_rx` open |
| Voice playback PCM/marker | Active phrase flag + `gate_rx` open after conditioning |
| Keypad DTMF | `dtmf_from_pad` open AND TX path policy allows |

## Voice-prompt RX conditioning (CO connected)

Exact order:

1. `0x29` (`RX_MUTE`)
2. `0x2A` (prompt begin) / `0x2B` (prompt end)
3. `0x28` (`RX_UNMUTE`)

## Processor → host

Supervision bytes: [`disconnect-supervision-emulation.md`](disconnect-supervision-emulation.md).

## Trace row

`{cycle, cmd_hex, gate_tx, gate_rx, sidetone_mode, gain_step}`.

## Tests

[`../test-plans/audio-routing-tests.md`](../test-plans/audio-routing-tests.md).

## Spec

[`../specs/audio-routing-device.spec.md`](../specs/audio-routing-device.spec.md).
