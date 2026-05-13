# Z180 ITC / IL (port 0x34 / 0x35) — CoinLine

## Ownership

- **MAME** `z180_device` owns internal **I/O** decode for **ITC** (**0x34** offset in internal window) and **IL** (**0x35**).
- The driver **does not** mirror these in a second shadow register.

## Read masks (reference)

Host utilities mirror MAME `z180_internal_port_read()` (see `millennium_z180_register_math.h`):

| Register | Read behavior |
| -------- | ------------- |
| **ITC** | `millennium_z180_itc_read_byte` — ones in **~0xC7** forced on read |
| **IL** | `millennium_z180_il_read_byte` — **`& 0xE0`** |

## Tracing

- **`z180-register-trace.jsonl`**: raw **`itc`**, **`il`** fields from **`build_z180_snapshot()`**.
- **`io-trace.jsonl`** internal tags: **`z180_itc`**, **`z180_il`** via `millennium_z180_internal_trace_tag`.

## Report artifact

- `build/generated/z180-itc-il-report.json`
