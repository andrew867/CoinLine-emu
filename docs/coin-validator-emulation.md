# Coin validator emulation

This document describes the coin-validator device. The Millennium-compatible terminal accepts coin pulses from the validator hardware and (on some SKUs) a serial coin-mech protocol.

## Purpose

Generate coin events the firmware reads as accepted denominations; produce error events for jams and rejects.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Pulse input | A discrete input that pulses for each coin event; pulse-train length encodes denomination. |
| Status port | Reports validator state: ready, jam, escrow level, return acknowledged. |
| Control port | Allows firmware to disable the validator, accept pending coins, or reject pending coins. |

For SKUs with serial coin-mech protocol, the pulse interface is replaced by a serial port carrying frames documented in the firmware evidence inventory.

## State machine

```
idle -> coin_inserted -> validating -> accepted -> escrow -> sweep -> idle
                                  \-> rejected -> return_chute -> idle
                                  \-> jam      -> jam_clear     -> idle
```

## Timing

- Pulse width: per board profile (`coin.pulse_width_us`).
- Inter-pulse gap: per board profile.
- Total event window: per fixture.

## Denominations

Default reference profile denominations: 5¢, 10¢, 25¢, $1.00. SKU profiles override `coin.denominations`.

## Interrupts

The validator may raise an interrupt on each pulse (per board profile). Default is polled.

## Fixtures

Coin events are injected via the scenario verb `insert_coin`:

```jsonc
{ "verb": "insert_coin", "denomination": 25 }
{ "verb": "insert_coin", "denomination": "jam" }
{ "verb": "insert_coin", "denomination": "reject" }
```

## Tests

- `tests/devices/test_coin_pulse_train.cpp` — verifies pulse-train decode for every denomination.
- `tests/devices/test_coin_jam.cpp` — verifies jam reporting.
- `tests/devices/test_coin_disable.cpp` — verifies firmware can disable the validator.

## Boot-milestone dependencies

- M5: first validator I/O observed.
- Coin-call scenarios depend on M10 (idle).

## Acceptance criteria

- Each declared denomination is recognized by the firmware.
- Jam / reject paths produce the firmware's expected behavior.

## Cross-references

- [`../specs/coin-validator-device.spec.md`](../specs/coin-validator-device.spec.md).
- [`../test-plans/coin-validator-tests.md`](../test-plans/coin-validator-tests.md).
