# Spec — Host bridge

## Purpose

Move modem UART byte traffic between the emulated terminal and an external endpoint (typically CoinLine Host) without synthesizing any application-layer behavior.

## Transports

| Transport | URL | Modes |
| --------- | --- | ----- |
| TCP | `tcp://host:port` | connect or listen |
| WebSocket | `ws://host:port[/path]` | connect or listen |
| Named pipe | `pipe://name` | listen on Unix sockets / Windows pipes |
| Serial | `serial:///dev/ttyUSB0` or `serial://COM3[,baud]` | direct |

Default in board profiles: `tcp://127.0.0.1:5210` (listen).

## Wire protocol

The bridge is a transparent byte pipe. UART TX bytes go onto the transport unchanged; transport bytes go into UART RX unchanged.

## Optional side-band protocol

When the peer signals support, an out-of-band side-band channel carries modem control signals. Side-band frames are framed JSON with a 4-byte length prefix:

```
+--------+--------+--------+--------+----------------------+
| len[3] | len[2] | len[1] | len[0] |   JSON frame         |
+--------+--------+--------+--------+----------------------+
```

Frame schema:

```jsonc
{
  "type": "control" | "negotiate",
  "ts":   "RFC 3339 UTC",
  "signal": "dcd|cts|rts|dtr|ring",
  "value":  true,
  "version": "1.0"
}
```

Negotiation:

- On TCP/WS connect, the bridge sends a `negotiate` frame.
- If the peer responds with a `negotiate` frame, side-band is enabled.
- Otherwise, side-band is disabled and modem signals follow board-profile defaults.

## Failure modes

| Failure | Behavior |
| ------- | -------- |
| Connection refused | Logged; firmware sees `no_carrier`. |
| Connection drop mid-call | Logged; firmware sees `carrier_lost`. |
| Side-band frame oversized | Connection terminated; logged. |
| Side-band JSON malformed | Connection terminated; logged. |

## Tests

- `tests/integration/test_host_bridge_loopback.cpp`
- `tests/integration/test_host_bridge_connection_drop.cpp`
- `tests/integration/test_host_bridge_sideband_negotiate.cpp`
- `tests/integration/test_host_bridge_no_sideband_default.cpp`

## Acceptance criteria

- Bytes round-trip with bit-for-bit fidelity.
- Modem control signals propagate when side-band is enabled.
- Defaults apply when side-band is disabled.
- The bridge introduces no protocol behavior beyond byte forwarding and signal propagation.

## License posture

The bridge is the only sanctioned cross-process communication channel between `coinline-emu` (GPL track) and `coinline` (MIT). It carries bytes only; no shared compiled artifact crosses the boundary.

## Cross-references

- [`modem-uart-device.spec.md`](modem-uart-device.spec.md).
- [`scenario-runner.spec.md`](scenario-runner.spec.md).
