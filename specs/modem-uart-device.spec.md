# Spec — Modem UART device

This spec defines the modem UART device contract. The host-bridge transport contract is in [`host-bridge.spec.md`](host-bridge.spec.md). See [`../docs/modem-uart-host-bridge.md`](../docs/modem-uart-host-bridge.md) for context.

## Purpose

Glue the Z180 ASCI to a modem state machine that reflects DCD / CTS / RTS / DTR semantics.

## Firmware-facing interface

The firmware sees standard Z180 ASCI registers per [`../docs/z180-internal-peripherals.md`](../docs/z180-internal-peripherals.md), plus board-profile-specific discrete signals.

## I/O ports / memory regions

| Interface | Source |
| --------- | ------ |
| ASCI registers | Z180 internal peripherals (per ICR base). |
| Modem control discretes | Per `fixtures/board/io-port-map.json` rows with `device = "modem"`. |

## State machine

```
idle -> dialing -> connected -> idle
idle -> ringing -> connected -> idle
* -> busy -> idle
* -> no_answer -> idle
connected -> carrier_lost -> idle
* -> noisy_line -> connected
```

## Timing behavior

- Bit clock from ASCI register programming (`CNTLB`, `ASTC`).
- Modem state durations are configurable per fixture.

## Interrupts

- ASCI 0 RX / TX via Z180 INT controller.
- DCD edge via `/INT0` (per board profile).

## MAME files

- `src/mame/coinline/millennium_modem.cpp/h`

## Fixture files

- `fixtures/modem/clean-connect.hex`
- `fixtures/modem/dropped-carrier.hex`
- `fixtures/modem/noisy-line.hex`

## Tests

- `tests/devices/test_modem_state_machine.cpp`
- `tests/devices/test_uart_transcript.cpp`

## Boot milestone dependencies

- M8 (UART/modem init).
- M11 (host call attempted).

## Acceptance criteria

- All declared modem states reachable.
- DCD / CTS / RTS / DTR semantics honored per board profile.
- TX / RX bytes round-trip via the host bridge with bit-for-bit fidelity.
