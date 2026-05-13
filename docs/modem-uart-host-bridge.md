# Modem UART and host bridge

This document describes the modem UART device and the host-bridge transport that connects emulated UART traffic to an external endpoint (typically CoinLine Host).

## Purpose

- Glue the Z180 ASCI 0 (or ASCI 1, per board profile) to a modem state machine that reflects DCD/CTS/RTS/DTR semantics.
- Voice-call scenarios also depend on the parallel **telephony processor** model described in [`audio-routing-emulation.md`](audio-routing-emulation.md) (mute/sidetone/volume), independent of UART payload framing.
- Bridge UART byte traffic to a TCP / WebSocket / named pipe / serial transport.
- Inject modem events (carrier loss, ringing, noise) for failure-mode tests.

## Firmware-facing interface

The firmware sees a standard Z180 ASCI register set ([`z180-internal-peripherals.md`](z180-internal-peripherals.md)) plus board-profile-specific modem-control discrete signals (DCD, CTS, RTS, DTR).

## Modem state machine

| State | Trigger | Notes |
| ----- | ------- | ----- |
| `idle` | reset | DTR low. |
| `dialing` | ATD or DTR raised | Tone / pulse dial sequence. |
| `ringing` | inbound ring | DCD asserts on connect. |
| `connected` | training complete | DCD high; CTS gated by host. |
| `busy` | far-end busy | Modem reports BUSY. |
| `no_answer` | timeout | Modem reports NO ANSWER. |
| `carrier_lost` | DCD drops | Triggered by `inject_modem_event "carrier_lost"`. |
| `noisy_line` | injected | RX bytes corrupted per fixture; CRC errors expected. |

## Modem control signals

| Signal | Direction | Default | Purpose |
| ------ | --------- | ------- | ------- |
| DCD | input | `false` | Carrier detect. |
| CTS | input | `true` | Clear to send. |
| RTS | output | n/a | Request to send. |
| DTR | output | n/a | Data terminal ready; firmware raises to dial. |

## Transport options

| Transport | URL form | Notes |
| --------- | -------- | ----- |
| TCP | `tcp://host:port` | Default; supports both connect and listen modes. |
| WebSocket | `ws://host:port[/path]` | For browser-based field tools. |
| Named pipe | `pipe://name` | Local lab use. |
| Serial | `serial:///dev/ttyUSB0` (Linux) or `serial://COM3` (Windows) | Direct to physical modem if present. |

The host bridge is **transparent** — it does not synthesize protocol responses. Bytes that go out the firmware's UART go onto the transport unchanged; bytes that arrive on the transport go into the firmware's UART unchanged. Modem signals (DCD/CTS/RTS/DTR) are encoded in optional side-band frames; defaults assume permanently-asserted DCD/CTS for lab loopback.

## Side-band frames (optional)

When the transport peer supports the side-band protocol, the bridge negotiates a control channel. Frame schema:

```jsonc
{
  "type": "control",
  "ts":   "RFC 3339 UTC",
  "signal": "dcd|cts|rts|dtr|ring",
  "value":  true
}
```

If the peer does not advertise side-band support, the bridge assumes static defaults from the board profile.

## Transcript export

Every connection produces a transcript entry:

```jsonc
{
  "direction": "tx|rx",
  "ts": "...",
  "bytes": "0x..."
}
```

Transcripts are part of evidence bundles.

## Fixtures

- `fixtures/modem/clean-connect.hex` — known-good connect sequence.
- `fixtures/modem/dropped-carrier.hex` — connect followed by carrier loss.
- `fixtures/modem/noisy-line.hex` — connect with intermittent CRC errors.

## Tests

- `tests/devices/test_modem_state_machine.cpp` — verifies state transitions.
- `tests/devices/test_uart_transcript.cpp` — verifies TX / RX byte fidelity.
- `tests/integration/test_host_bridge_loopback.cpp` — verifies TCP loopback round-trip.
- `tests/integration/test_host_bridge_carrier_loss.cpp` — verifies carrier-loss injection.
- `tests/integration/test_host_bridge_noisy_line.cpp` — verifies noisy-line injection.

## Boot-milestone dependencies

- M8: UART/modem init observed.
- M11: host call attempted (TX bytes appear at the host bridge).

## Acceptance criteria

- Firmware progresses through the full modem state machine under `clean-connect.hex`.
- Carrier loss triggers the firmware's expected recovery path.
- Bytes round-trip through the host bridge with bit-for-bit fidelity.

## Cross-references

- [`../specs/modem-uart-device.spec.md`](../specs/modem-uart-device.spec.md).
- [`../specs/host-bridge.spec.md`](../specs/host-bridge.spec.md).
- [`../test-plans/modem-uart-tests.md`](../test-plans/modem-uart-tests.md).
- [`../test-plans/host-integration-tests.md`](../test-plans/host-integration-tests.md).
