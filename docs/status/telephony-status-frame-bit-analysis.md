# Telephony status frame (0xC0) — bit-oriented notes

## Frame layout

| Offset (after opcode) | Field |
|----------------------|--------|
| 0 | Length (must be **8**) |
| 1–2 | `total_fail_to_pots_calls` (little-endian word) |
| 3–4 | `call_duration` (little-endian word) |
| 5 | `call_state` |
| 6 | Checksum byte |

Checksum: rolling **8-bit sum** of opcode, length, and all payload octets equals the final checksum octet (same rule used by the CSIO test parser in the driver).

## Call state nibble

Lower bits encode voice path state (**idle**, dialing, established, etc.). Emulator uses **idle (0)** in the default modeled payload.

## Relation to M7C

Valid **0xC0** proves the IPC decoder accepts a status message; it does **not** by itself satisfy the milestone definition until firmware-driven UI leaves the stall banner.
