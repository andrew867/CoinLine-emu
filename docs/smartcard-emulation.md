# Smart-card emulation

This document describes the smart-card / memory-card device. The Millennium-compatible terminal supports ISO/IEC 7816 memory cards (and, on some SKUs, microprocessor cards). The exact protocol stack is per board profile.

## Purpose

Provide an emulated contact card driven by the terminal's real power, reset
and serial clock signals. The synchronous contact interface is implemented.
The earlier byte-oriented ATR/APDU test model remains an isolated device
test abstraction; it is not evidence of an asynchronous card application
working through the native terminal.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Reset / clock control | Discrete signals controlling card power, reset, and clock. |
| Data port | Serial line for ATR / APDU / memory-card responses. |
| Status port | Card-present, ATR-received, response-ready, error flags. |

The native synchronous interface uses the following board mappings:

| Signal | Earlier control board | Later control board |
| --- | --- | --- |
| Power request | PIO A bit 6 | Port E bit 3, latched by rising port F bit 1 |
| Reset | PIO C bit 7 | Port E bit 2 |
| Manual clock | PIO C bit 6 | Port D bit 3, enabled with port E bit 0 clear |
| Data input | PIO C bit 2 | Port H bit 1 |
| Data release | Bidirectional contact pull-up | Port D bit 2 |

The card exposes its first serial bit after a reset pulse containing a clock
edge, then advances one bit on each rising clock with reset inactive. The
data is read MSB first. Removal clears its power and reset state. No card
data is returned through a fabricated byte FIFO on the native bus.

Optional synthetic memory fixtures may enable bounded one-way bit
programming. Programming requires an explicit permitted address interval
and minimum clock-hold duration; an interrupted or out-of-range operation
does not change the card. Repeated writes cannot restore a cleared bit.
Reinsertion of the same fixture retains its in-process memory state. The
default contact fixture is read-only, and the emulator does not write
changes back to the input file. Application-specific erase/carry rules,
credentials and monetary balance validation are not implemented by this
generic programming model.

## State machine

```
absent -> present_powered_off -> present_powered_on -> reset -> atr -> idle
idle  -> apdu_request -> apdu_response -> idle
idle  -> remove -> absent
```

For memory cards, the `apdu_*` states are replaced by synchronous read / write states per the memory protocol (e.g., I²C-style 3-wire).

## Timing

- ATR delay: per ISO 7816-3 timing (varies with card; per fixture).
- APDU response delay: per fixture.
- Reset duration: per ISO 7816-3.

## Interrupts

Per board profile. Default behavior is polled.

## Fixtures

- `fixtures/cards/smartcard-sync-test.json` is the default synthetic contact
  test fixture. Its nonuniform first byte exercises every contact data bit;
  it is not a funded payment card or a valid issuer credential.
- `fixtures/cards/smartcard-valid.json` is the earlier byte-model test fixture.
- `fixtures/cards/smartcard-empty.json` — empty / blank card with default ATR.

Fixture format:

```jsonc
{
  "type": "smartcard",
  "protocol": "memory" | "t0" | "t1",
  "atr": "0x3B 0x...",
  "memory": "...",
  "apdu_responses": [...],
  "atr_delay_us": 4000
}
```

## Tests

- `tests/devices/test_smartcard_atr.cpp` — verifies ATR delivery.
- `tests/devices/test_smartcard_apdu.cpp` — verifies APDU exchange (when applicable).
- `tests/devices/test_smartcard_memory_read.cpp` — verifies memory-card read.
- `tests/devices/test_smartcard_protocol_negotiation.cpp` exercises rising
  clock edges, reset qualification, later-board power latch, earlier-board
  direct supply, serial ordering and removal. It also verifies the bounded
  synthetic programming rules, including aborted and undersized pulses.
- `tools/acceptance/smartcard_pin_probe.py` uses the native board I/O and
  actual card insertion input to verify a synthetic memory bit changes
  only after a timed programming pulse, survives reset and reinsertion,
  and leaves its read-only neighbour unchanged.

On NLA1X08, maintenance item 728 (Check Smart Card) displayed **Smart Card
Test Okay** twice after real insert/remove input. The eight contact reads
matched the fixture and power was removed afterward. The runtime call path
correctly rejected the same unfunded contact fixture. Evidence is in
`build/firmware-acceptance/graphic-firmware/20261125T053617.302424Z/`.
For a private bench run, use `graphic_session.py --card-trace` to capture
the pin transitions and `--smart-card` or `--mag-card` to select local
test fixtures. No test-card credentials or private run captures should be
committed.

## Boot-milestone dependencies

- M5: first smart-card I/O observed.
- Smart-card scenarios depend on M10 (idle).

## Acceptance criteria

- Synchronous reset and contact reads pass the installed firmware's Craft
  test and full INSTALL test independently.
- Asynchronous ATR and APDU exchange through the native board remain
  separate qualification gates.
- Removing the card triggers the firmware's expected error path.
- An agreed lab-only funded card application must validate independently,
  complete a rated call and demonstrate the corresponding debit or simulated
  host transaction. The generic contact test is not sufficient.

## Cross-references

- [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md).
- [`../test-plans/smartcard-tests.md`](../test-plans/smartcard-tests.md).
- [`card-reader-emulation.md`](card-reader-emulation.md).
