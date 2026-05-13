# TP-CP Protocol State Machine Spec

## States

- `reset_pending`: TP link not yet enabled.
- `boot_ack_visible`: first TP boot code (`0x70`/`0x72`) observable by CP.
- `init_dialogue`: CP queries hook/power/error/status sequence.
- `runtime_active`: periodic health polling (`0x31`, `0x38`) and UI events.
- `timeout_latched`: CP-visible not-responding timeout path.

## CP->TP Command Classes

- Query commands:
  - `0x30` hook status
  - `0x31` telephony status
  - `0x33` power status
  - `0x38` error report
- Runtime controls:
  - `0x10..0x2f`, `0x40..0x46` accepted as control/audio/call state signals
- Var-length:
  - `0xC0` configuration frame with length/checksum handling

## TP->CP Responses

- Single byte:
  - hook state (`0x6C`/`0x6E`), power (`0x7A`), line (`0x80`)
- Framed:
  - status (`0xC0 len=8`)
  - error report (`0xC4 len=4`)
  - extended status (`0xC2 len=0E`)

## Boot Contract

- Contract satisfied when CP has observed required readiness sequence:
  - power ack + hook + status/error report progression
- Bootstrap keepalive allowed after boot ack to avoid deadlock.

## Timeout/Retry Policy

- Runtime polls advance counters for:
  - timeout misses
  - retries
  - relatch guards
- Latch occurs only when grace windows and post-clear guards do not apply.

## Invariants

- No response frame without checksum-valid construction.
- No status injection bypassing CSI/O queue.
- No CP-side “ready” latch unless TP protocol evidence exists.