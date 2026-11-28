# Smart-card emulation

This document describes the smart-card / memory-card device. The Millennium-compatible terminal supports ISO/IEC 7816 memory cards (and, on some SKUs, microprocessor cards). The exact protocol stack is per board profile.

## Purpose

Provide an emulated contact card driven by the terminal's real power, reset
and serial clock signals. Native synchronous contacts are implemented on the
earlier and later board mappings. The later-board customer socket also routes
asynchronous traffic through the actual Z180 ASCI0 pins. The synthetic
read-only application qualifies transport, not historical payment products.

## Firmware-facing interface

| Interface | Description |
| --------- | ----------- |
| Reset / clock control | Discrete signals controlling card power, reset, and clock. |
| Data port | Serial line for ATR / APDU / memory-card responses. |
| Status inputs | Card presence and contact level; asynchronous receive/error status belongs to the terminal's actual ASCI0. |

The native synchronous interface uses the following board mappings:

| Signal | Earlier control board | Later control board |
| --- | --- | --- |
| Power request | PIO A bit 6, supply-enable pulse latched on its rising edge | Port E bit 3, latched by rising port F bit 1 |
| Reset | PIO C bit 7, active low at latch | Port E bit 2 |
| Manual clock | PIO C bit 6, active low at latch | Port D bit 3, enabled with port E bit 0 clear |
| Data input | PIO C bit 2 | Port H bit 1 |
| Data release | Bidirectional contact pull-up | Port D bit 2 |

The card exposes its first serial bit after a reset pulse containing a clock
edge, then advances one bit on each rising clock with reset inactive. The
data is read MSB first. Removal clears its power and reset state. No card
data is returned through a fabricated byte FIFO on the native bus.
Synchronous fixtures also leave the compatibility character FIFO idle and
cannot negotiate an asynchronous protocol. Their ATR placeholder does not
represent a transmitted character or a successful asynchronous activation.

The address within the physical contact stream is `8 * byte_offset + bit`,
where physical bit zero has byte weight `0x80`. The separate read-only
four-byte laboratory fixture `smartcard-sync-address-test.json` contains
`96 3C A7 00`. It detects reversed bits, shifted addresses and byte-boundary
errors. Each native board mapping is tested by resetting to zero, reading
the complete pattern, advancing to the second byte, resetting again, reading
past the declared end and removing/reinserting the card. Out-of-range contact
reads on this fixture release the data line rather than wrapping. Its
capacity and contents are synthetic, not a claimed GPM103 or GPM416 device
geometry. No family-specific carry, erase or protected-area programming is
inferred from the generic contact test.

Historical reader compatibility references distinguish the GPM103 counter
memory family from the GPM416 intelligent EEPROM family; neither establishes
the original chip's capacity or protected geometry. The terminal's nominal
programming wait is one 10 ms executive tick, and the GPM416 software can
round its configurable wait up to whole ticks. These software delays do
not establish the physical card's minimum programming pulse. The independent
synthetic programming probe accepts `--program-us 10000` to test bounded
non-payment memory switching on that time scale without identifying the
fixture as either historical card family.

### GPM103-family EEPROM contact bench

`fixtures/cards/smartcard-sync-gpm103-contact.json` is a deliberately
arbitrary 16-byte synchronous test memory. The final byte contains two
explicitly writable bench bits with a nominal 10 ms programming interval.
It is selectable as the primary contact fixture on either board. This
interval is a bench setting, not a measured programming-pulse limit.
Historical reader documentation identifies GPM103 as a synchronous memory
family ([Gemplus GemCore controller manual](https://ifd-gempc.apdu.fr/e5221031.pdf),
[ACS ACR30 reference manual](https://www.acs.com.hk/download-manual/1184/REF_ACx30.pdf));
the exact physical chip capacity is not established by those classifications.

Run `tools/acceptance/smartcard_memory_order_probe.py --gpm103-contact`
with the native emulator executable. It verifies all fixture bytes, address
seeking, out-of-bounds reads, short-pulse rejection, programming after a
held clock, read-only bounds, interrupted programming on removal, reinsertion
and restart persistence through the board's real contact pins. The fixture
does not contain application-specific data. The optional native Craft test
uses the same fixture with `smartcard_craft_probe.py --gpm103-contact`.

For the earlier board, the firmware drives a low/high/low supply-enable
pulse and subsequently clocks the contact while that pin remains low. The
model retains supply across the falling edge; a card removal or device reset
clears the latch. Its reset and clock latch outputs are inverted on the card
side, so the idle PIO C levels are high. The detailed analogue discharge
circuit is not qualified.

Optional synthetic memory fixtures may enable bounded one-way bit
programming. Programming requires an explicit permitted address interval
and minimum clock-hold duration; an interrupted or out-of-range operation
does not change the card. Repeated writes cannot restore a cleared bit.
Loss of supply or contact selection also cancels an open programming pulse,
even when its elapsed duration would otherwise permit a write.
Reinsertion of the same fixture retains its memory state. The configured
writable test fixture also persists across emulator restart in a separate
card NVRAM device file, keyed to its fixture digest. The default contact
fixture is read-only, and the emulator does not write changes back to the
input file. Application-specific erase/carry rules,
credentials and monetary balance validation are not implemented by this
generic programming model.

### Short synchronous memory test image

`fixtures/cards/smartcard-sync-13byte-bench.json` is a separate 13-byte
non-payment test image. It uses the existing synchronous socket and the
same bounded one-way programming model as the contact bench, with two
writable bits at the start of the final byte. It exercises the shorter
memory boundary without replacing the original 16-byte fixture. It has no
issuer identity, payment authentication, or stored-value semantics.

The fixture is included in the portable package and is selectable as a
contact fixture in `[cards]`. Test both native board mappings with
`tools/acceptance/smartcard_memory_order_probe.py --13-byte-bench` and
`--mame` pointing to the native executable. The probe checks all 13 bytes,
out-of-range reads, bounded programming, removal during programming and
persistence on reinsertion and restart, independently of the 16-byte bench.
These checks do not qualify an application or payment path.

### GPM103 lab image readback

`fixtures/cards/smartcard-sync-gpm103-lab.json` is a separate, read-only
13-byte frozen lab image. It is included in the portable package and does
not replace the GPM103-family contact bench or the independent 13-byte bench.
Run `tools/acceptance/smartcard_memory_order_probe.py --gpm103-lab` with
`--mame` pointing to the native executable. The probe compares the complete
13-byte synchronous contact readback with the fixed fixture bytes on both
board mappings. It performs no programming or application-path checks.
This qualifies lab fixture readback only; OEM table acceptance and any
application-path or commercial interoperability remain unqualified.

The asynchronous socket uses generated card clock, supply latch and reset
selection. Direct and inverse ATR convention, T=0 procedure bytes, T=1
LRC blocks and chaining, and bounded PPS negotiation are qualified by native
synthetic Z180 transactions. The earlier coin-ASCI0 path and later coin UART
remain separate from customer-card traffic. The lab application is read-only;
unsupported instructions are rejected.

### Internal contact modules

The later SAM daughterboard has three separate internal contact positions,
independent of the customer card and the ADSI/graphical expansion. The same
ASCI0 serial adapter follows the physical selector, rather than creating a
UART for each device. The following later-board controls have independent
effects on each module:

| Socket | Selector (`0xC1` bits 1:0) | Reset, active low | Supply, active low | Clock enable |
| --- | --- | --- | --- | --- |
| 0 | 0 | `0x43` bit 0 | `0x43` bit 3 | `0x63` bit 3 |
| 1 | 1 | `0x43` bit 1 | `0x63` bit 1 | `0x63` bit 4 |
| 2 | 2 | `0x43` bit 2 | `0x63` bit 2 | `0x63` bit 5 |

The shared async gate is active low at `0xC0` bit 3. The model uses
`0xC2` bit 1 as an active-high common module clock enable, in addition
to the individually enabled clock. The synthetic CPU probe exercises that
interpretation; physical clock polarity measurement remains outstanding.
Selector 3 retains the separate customer card controls. Disconnecting an
endpoint flushes its pending serial transfer and releases the UART line.
All internal sockets are absent by default. The optional laboratory socket
fixtures accept only the generic read-only async application; they cannot
perform payment authentication, loading or debits.

For a local diagnostic run, `graphic_session.py --sam-pin-trace` records
bounded selector/latch transitions; `--sam-slot0`, `--sam-slot1` and
`--sam-slot2` accept separate non-payment fixture paths. Fixture hashes
are retained in the local run directory. The native `smartcard_async_probe.py
--protocol slots` exercises all three internal positions with distinct
immutable values, then selects and reads the customer contact through
the real Z180 ASCI0. This qualifies routing and serial transport, not a
historical scheme module or its association state.

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
- `fixtures/cards/smartcard-sync-address-test.json` is a four-byte read-only
  address and bit-order fixture, used on both native motherboard maps.
- `fixtures/cards/smartcard-valid.json` is the earlier byte-model test fixture.
- `fixtures/cards/smartcard-async-t0-lab.json` and
  `fixtures/cards/smartcard-async-t1-lab.json` are native asynchronous
  read-only transport fixtures, not issuer cards.

Fixture format:

```json
{
  "type": "smartcard",
  "protocol": "async",
  "application": "lab-readonly",
  "reset_policy": "external",
  "atr_hex": "3B 00",
  "memory_hex": "54455354",
  "atr_delay_us": 4000,
  "apdu_response_delay_us": 1000
}
```

## Tests

- `tests/devices/test_smartcard_protocol_negotiation.cpp` exercises rising
  clock edges, reset qualification, the later-board power latch, earlier-board
  supply-enable pulse, serial ordering and removal. It also verifies the bounded
  synthetic programming rules, including aborted and undersized pulses.
- `tools/acceptance/smartcard_pin_probe.py` uses the native board I/O and
  actual card insertion input to verify a synthetic memory bit changes
  only after a timed programming pulse, survives reset and reinsertion,
  and leaves its read-only neighbour unchanged.
- `tests/devices/test_smartcard_async_transport.cpp` verifies generic ATR,
  read-only T=0/T=1 behavior, invalid frames, selection/removal and PPS.
- `tools/acceptance/smartcard_async_probe.py` uses the physical ASCI0
  serial boundary for those positive synthetic transport cases.
- `tools/acceptance/smartcard_craft_probe.py` exercises the actual
  maintenance menu, contact insertion/removal and return to service on
  a copy of an installed terminal.

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
- Native asynchronous ATR and read-only APDU transport are qualified for
  the tested generic profiles; historical application validation and all
  speed/timing combinations remain separate qualification gates.
- Removing the card triggers the firmware's expected error path.
- An authorized nonredeemable lab card application must validate independently,
  complete a rated call and demonstrate the corresponding debit or simulated
  host transaction. The generic contact test is not sufficient.

## Cross-references

- [`../specs/smartcard-device.spec.md`](../specs/smartcard-device.spec.md).
- [`../test-plans/smartcard-tests.md`](../test-plans/smartcard-tests.md).
- [`card-reader-emulation.md`](card-reader-emulation.md).
