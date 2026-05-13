# Test plan: Card, smartcard, coin

## Unit tests

`test_card_*`, `test_smartcard_*`, `test_coin_*` under `tests/devices/`; integrations `test_card_call`, `test_coin_call`.

## Failure criteria

Card/coin acceptance without simulated hardware events (insert, swipe, pulse train).

## Optional traces

`card-trace.jsonl`, `coin-trace.jsonl` when harness wires JSONL emitters.
