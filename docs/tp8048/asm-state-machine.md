# TP ASM State Machine Notes

## States

| State | Meaning |
| --- | --- |
| `reset_pending` | TP has not yet completed boot/link readiness. |
| `boot_ack_visible` | TP has queued `0x70` or `0x72`. |
| `init_dialogue` | CP is querying hook, power, error, and status. |
| `runtime_active` | CP periodic health queries and UI events are active. |
| `timeout_latched` | TP missed health and must report/not clear fault until recovery. |
| `craft_window` | Optional internal trace state after craft digits observed. |

## Query handling

| CP command | TP behavior |
| --- | --- |
| `0x30` | emit `0x6C`/`0x6E` hook state |
| `0x31` | emit `C0 len=8` status frame |
| `0x33` | emit `0x7A` power status |
| `0x38` | emit `C4 len=4` error report |
| `0xC0` | accept config frame, queue `0x74`, then readiness frames if policy requires |

## Keypad path

The keypad is TP-side. The assembly reads a decoded low-nibble key from P1 in the first-pass firmware, but the wrapper may implement the real mux/scan pattern and present a stable decoded value to the firmware.

Key requirements:

1. TP sees the key edge.
2. TP debounces it.
3. TP emits a key opcode to CP.
4. CP consumes the key opcode.
5. Craft truth chain proves `2727378` reached CP.

## Hook path

Hook transition emits both transition and state bytes:

- on-hook transition: `0x60`, then `0x6C`
- off-hook transition: `0x62`, then `0x6E`

## Runtime health

The firmware draft avoids unsolicited C0 spam. Health is reset only after CP-initiated query/response transactions.

## Tone path

The firmware writes tone intent via derivative-register stubs or observable tone mode. The C++ backend should implement exact generated tones and routing.

