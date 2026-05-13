# Audio trace format specification

## Scope

Structured logs for audio subsystem debugging and CI. **Encoding**: UTF-8 JSON Lines (`.jsonl`), one object per line.

All audio subsystem devices (`millennium_voiceware_device`, `millennium_telephony_device`, `millennium_audio_route_device`, `millennium_supervision_device`, `millennium_audio_device`) emit rows compatible with this schema.

## Schema version

Top-level field **`schema_version`**: `"coinline.audio_trace/v1"`.

## Common fields

| Field | Type | Required | Description |
| ----- | ---- | -------- | ----------- |
| `schema_version` | string | yes | Format version |
| `timestamp_emulated_ns` | number | yes | Monotonic emulated time |
| `cycle` | number | yes | Master cycle counter (CPU or agreed tick) |
| `pc` | string | yes | Program counter hex `"0x1234"` when CPU-initiated; `"0x0000"` allowed for device-timer events |
| `event_type` | string | yes | Event enum (tables below) |
| `device` | string | yes | `voiceware`, `telephony`, `audio_route`, `supervision`, `alerter`, `audio` |
| `port` | string | no | Hex port address when event originated from Z180 I/O |
| `direction` | string | no | `read` / `write` |
| `value` | string | no | Hex raw value |
| `decoded_command` | object | no | Structured decode (e.g. `{ "hex": "0x26", "effect": "TX_UNMUTE" }`) |
| `previous_state` | object | no | Prior composite state snapshot |
| `new_state` | object | no | New composite state snapshot |
| `call_state_if_known` | string | no | Only when legitimately derived |
| `route_state` | string | no | Route enum |
| `mute_state` | object | no | `{ "mic": bool, "ear": bool }` or TX/RX fields |
| `prompt_id` | object | no | `{ "bank": number, "phrase": number }` |
| `disconnect_event` | string | no | Supervision classification |
| `status_code_hex` | string | no | Raw supervision / telephony status byte |
| `notes` | string | no | Diagnostics |
| `compatibility_flag` | string | no | `compatibility_validation_required` |

## Event type taxonomy (minimum — implement all rows)

### Voiceware (`device`: `voiceware`)

| `event_type` | Description |
| -------------- | ----------- |
| `voice_reset_edge` | Reset bit asserted or released |
| `voice_segment_start` | Playback started |
| `voice_segment_complete` | Normal completion |
| `voice_fault` | Watchdog / illegal sequence |

### Telephony bridge (`device`: `telephony`)

| `event_type` | Description |
| -------------- | ----------- |
| `telephony_host_to_processor_byte` | Raw byte toward processor |
| `telephony_processor_to_host_byte` | Raw byte from processor (feeds supervision) |
| `telephony_command_decode` | Byte matched `audio-routing-state-map` command |

### Audio route (`device`: `audio_route`)

| `event_type` | Description |
| -------------- | ----------- |
| `route_change` | Call/route mux changed |
| `mute_change` | TX/RX/sidetone mute tuple changed |
| `route_conflict_resolved` | Arbitration between voice prompt vs line |

### Supervision (`device`: `supervision`)

| `event_type` | Description |
| -------------- | ----------- |
| `supervision_status_code` | Mapped `processor_to_host` code observed |
| `supervision_status_read` | CPU read of supervision aggregate (when implemented) |
| `disconnect_event` | Typed disconnect |
| `supervision_timeout` | Timer expiry |

### Alerter (`device`: `alerter` or `audio`)

| `event_type` | Description |
| -------------- | ----------- |
| `alerter_ready` | Device ready after reset |
| `alerter_gpio_write` | Port/bit affecting alerter |
| `alerter_tone_start` | Tone phase on |
| `alerter_tone_end` | Tone phase off |

## Output files

| File | Filter |
| ---- | ------ |
| `audio-trace.jsonl` | Superset: all rows |
| `voiceware-trace.jsonl` | `device == voiceware` |
| `supervision-trace.jsonl` | `device == supervision` |
| `alerter-trace.jsonl` | `device in { alerter, audio }` AND alerter event types |

## Validation

CI loads JSONL line-by-line (reject malformed); optional JSON Schema under `specs/schemas/` in a later tranche.

## Firmware-proof tests

Tests that assert **`event_type`** sequences from **real firmware** must obtain JSONL from a run of **`coinline-mame.exe`** / **`mamecoinline.exe`** — see [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md).
