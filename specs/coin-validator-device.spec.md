# Spec — Coin validator device

This spec defines the coin-validator contract. See [`../docs/coin-validator-emulation.md`](../docs/coin-validator-emulation.md) for context.

## Purpose

Generate coin events the firmware reads as accepted denominations; produce error events for jams and rejects.

## Firmware-facing interface

| Interface | Direction | Notes |
| --------- | --------- | ----- |
| Pulse input | R | Discrete input that pulses for each coin event. |
| Status port | R | ready, jam, escrow_level, return_acked. |
| Control port | W | disable, accept_pending, reject_pending. |

For SKUs with serial coin-mech protocol, replace pulse with a serial port.

## I/O ports

Per `fixtures/board/io-port-map.json` rows with `device = "coin"`.

## State machine

```
idle -> coin_inserted -> validating -> accepted -> escrow -> sweep -> idle
                                  \-> rejected -> return_chute -> idle
                                  \-> jam      -> jam_clear     -> idle
```

## Timing behavior

- `pulse_width_us` per board profile.
- `inter_pulse_gap_us` per board profile.

## Denominations

Per board profile (`coin.denominations`). Reference: `[5, 10, 25, 100]`.

## Interrupts

Per board profile.

## MAME files

- `src/mame/coinline/millennium_coin.cpp/h`

## Fixture files

Driven by scenario verb `insert_coin`.

## Tests

- `tests/devices/test_coin_pulse_train.cpp`
- `tests/devices/test_coin_jam.cpp`
- `tests/devices/test_coin_disable.cpp`

## Boot milestone dependencies

- M5; coin-call scenarios depend on M10.

## Acceptance criteria

- Each declared denomination is recognized.
- Jam / reject paths produce the firmware's expected behavior.
- Disabling the validator prevents acceptance.
