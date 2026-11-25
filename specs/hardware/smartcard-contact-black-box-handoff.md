# Contact smart-card hardware and application separation

## What is already qualified

Both the earlier and later control-board profiles have passed the native synchronous Smart Card Test. The later profile has also passed two selector values, repeated insert/remove trials, and bounded synthetic memory programming with persistence across removal. The fixture is an unfunded test contact card. Its rejection by the normal payment interface is correct. Neither the Craft contact test nor an ATR is evidence of successful application acceptance, authorization, charging or settlement.

Native generic asynchronous customer-card operation, including ATR, T=0 and T=1 transport, is now qualified. The three internal positions have also passed a synthetic native ASCI0 probe with independent fixtures. Scheme-specific application and physical module qualification remain separate objectives. Keep the working synchronous contact implementation intact.

## Contact hardware

The reader can present either a clock-addressed memory card or a processor card. Both use the same customer-facing connector. They must not be modeled as interchangeable byte FIFOs. The later control board also has three selectable internal asynchronous module positions, distinct from the user card and from each other.

### Later board: observable ports

| Port | Mask | Direction | Meaning |
| --- | ---: | --- | --- |
| `0xC0` | `0x08` | Write | Clear to select asynchronous contact devices. The same address has a distinct status-read function. Preserve other latch bits. |
| `0xC1` | `0x03` | Write | Async routing selector. Values `0`, `1`, `2` select internal module positions; `3` selects the customer card. This is the hardware selector, not a software owner's numerical identifier. |
| `0xC1` | `0x04` | Write | Manual card data output; the card I/O contact is bidirectional and released rather than driven against the other participant. |
| `0xC1` | `0x08` | Write | Manual synchronous card clock, high when asserted. |
| `0xC2` | `0x01` | Write | Enable generated asynchronous customer-card clock. Clear for the manual synchronous clock path. |
| `0xC2` | `0x04` | Write | Customer card reset level, set high or clear low. |
| `0xC2` | `0x08` | Write | Customer card supply enable request. |
| `0xC3` | `0x01` | Write | Extra manual card control line. |
| `0xC3` | `0x02` | Write | Latch-clock edge used with the card-supply request. This output is also used for warning-tone transitions; preserve shared-port semantics and do not treat every toggle as an independent card insertion or reset. |
| `0xC2` | `0x02` | Write | Shared internal module clock control; the model treats set as enable, pending physical polarity confirmation. |
| `0x43` | `0x07` | Write | Internal module reset bits 0, 1 and 2, active low. |
| `0x43` | `0x08` | Write | Socket 0 supply, active low. |
| `0x63` | `0x06` | Write | Socket 1 and 2 supply bits 1 and 2, active low. |
| `0x63` | `0x38` | Write | Individual socket clock enables, bits 3, 4 and 5. |
| `0xC0` | `0x02` | Read | Synchronous customer-card data contact. Other independent status bits remain intact. |

The card contact has been observed with more than one selector value in synchronous operation. Do not constrain the manual clock/data path to the asynchronous selector value. The selector matters to the asynchronous UART route. The earlier board uses its separate A/C latch mapping and must not inherit the later-board port map. Its native synchronous tests have already passed.

The customer card and individual internal module positions have separate power, reset and clock conditions. The host processor uses asynchronous serial channel 0 for this interface; selection can change while the CPU serial channel is disabled. Do not permanently bind that channel to the coin path on every board. Select the actual endpoint from board signals and the configured hardware generation; the coin and management-modem transports remain independent of card traffic when their board routes are selected. Track pending bytes and serial state per endpoint and discard the active transfer on removal, power-off or reset. Do not leak one slot's responses to another slot. The choice of external module, contact-level voltage and physical connector wiring requires hardware verification.

## Activation and ATR

For an asynchronous user card, the terminal selects the contact, holds reset and clock in their initial states, enables power, and allows a 10 ms power stabilization interval. It then enables the generated card clock and prepares the serial receiver. It can first listen for a card with an internal reset arrangement; if no response is detected within its 20 ms start-check interval, it raises reset and checks another interval for a conventional externally reset card. Respond only under the fixture's declared reset policy, not merely on insertion or on a host-side request to read an ATR.

The archived design uses a nominal 3.57954 MHz card clock and initially approximately 9,600 baud. At default F=372 and D=1, one elementary time unit (ETU) is approximately 103.924 microseconds. An externally activated card's response-start range is expressed in card-clock cycles: 400 cycles is about 111.746 microseconds; 40,000 cycles is about 11.175 ms. A test fixture may choose a fixed response delay strictly within that range after the appropriate activation event. The firmware checks for at least one byte after 20 ms, permits an overall ATR interval of 2 seconds, and contains an inter-character timeout. These are terminal software deadlines, not a reason to delay every ATR by 20 ms or 2 seconds.

Implement characters at the real serial input/output boundary, with start, data, parity and guard spacing determined by the programmed configuration. The initial direct convention uses even parity. The inverse convention is supported through logical bit conversion and a different physical UART parity configuration; an initial character with unusual parity may be intentional. Do not fake a successful ATR by setting a ready bit or bypassing the receiver. Record the actual bytes and errors. The negotiated F/D baud, guard time, T=0 waiting time and T=1 timing parameters should follow the parsed interface characters; support changing serial parameters rather than assuming all cards remain 9,600 baud. Verify protocol/parameter selection exchanges on the native path before claiming PPS support.

### Synthetic, non-payment ATR vectors

| ATR (hex, canonical logical octets) | Purpose | Expected transport behavior |
| --- | --- | --- |
| `3B 00` | Minimal direct-convention T=0 ATR, no historical bytes | Detect async card, default timing, T=0. Card application may be rejected as unknown. |
| `3B 80 01 81` | Minimal direct-convention T=1 ATR, with checksum | Detect async card; T=1 indicated; exclusive-or of the bytes following TS is zero. Confirm the actual terminal's profile negotiation. |
| `3B 80 01 80` | Deliberately incorrect T=1 check byte | Reject the invalid ATR rather than classifying the transport as valid. |
| No response | Synchronous probe test | After the async reset attempts time out, allow the real terminal to switch to manual clocking. |

The examples are generic standards-level transport fixtures. They are not historical issuer ATRs. Historical bytes, valid timing and protocol identification do not prove a card belongs to a payment application.

## Implement transport before any application

### T=0

The host sends a five-byte command header (class, instruction, P1, P2, P3). The card returns actual procedure characters to control data transfer; NULL (`60`) extends waiting, an appropriate instruction acknowledgement requests or permits data transfer, and the final SW1/SW2 are status bytes. Separate command data from expected response data and preserve direction changes on the half-duplex contact. The terminal may issue GET RESPONSE as a follow-up; it is not safe to queue an entire interpreted APDU response without the procedure-character handshake. Response delays, parity errors, timeout and removal must be testable.

### T=1

Implement the block transport separately from APDU processing: node address, protocol control byte, information-field length, information field, and error-detection code. Support I-block sequence numbers, chaining, R-block acknowledgements/recovery, and S-block parameter/control exchanges as required by the negotiated profile. Initially qualify a single I-block with LRC, then sequence, chaining, checksum failure, and retry. Support CRC only where negotiated. The APDU application receives a complete validated payload, not arbitrary serial bytes. Preserve timeout and retry state per inserted card.

### Example independent lab application

The following fixture is deliberately unrelated to any payment scheme. Use it in an emulator component harness or an explicitly configured non-payment card test utility. It is not expected to be recognized by an unmodified historical payment application.

| Command APDU (hex) | Application result (hex) | Meaning |
| --- | --- | --- |
| `00 A4 04 00 05 F0 4C 41 42 01` | `90 00` | Select a synthetic local-only application identifier. |
| `00 B0 00 00 04` | `54 45 53 54 90 00` | Read the four ASCII characters `TEST` from its immutable test object, after selection. |
| Select any unrecognized application | `6A 82` | Unknown application/file. |
| Unsupported instruction | `6D 00` | Instruction not implemented. |

For the T=0 select example, wait for the header, return the instruction procedure acknowledgement, receive its five data bytes, then return the status. For the T=0 read example, return the instruction procedure acknowledgement, four data bytes and status. The actual characters must traverse the serial pin. In a fresh T=1 exchange with LRC, the read command can be carried in `00 00 05 00 B0 00 00 04 B1`; a successful first response can be `00 00 06 54 45 53 54 90 00 80`. These vectors assume the synthetic file is selected in the harness and an initial sequence number of zero in each direction. The final octet makes the exclusive-or over the entire block zero.

## Synchronous cards

The synchronous memory path is a sequence of genuine contact operations. Power and reset establish a known address; clock edges advance a bit address; the data line presents the selected memory bit, with bytes assembled MSB first. The qualified Craft probe samples a nonuniform first byte, not a canned `pass` flag. Missing power, removal or an absent card must release the line. The already-implemented timed test programming is a bounded one-way mutation of an explicitly writable, non-payment memory region, not a monetary debit algorithm. It must continue to pass read/clock/reset/removal tests and retain the programmed test bit across legitimate reinsertion. Never treat such a bit as a currency counter.

For new fixtures, distinguish a raw contact-test memory image from an application-specific provider. Use arbitrary nonfunded data patterns for contact tests; e.g. ascending nonuniform test bytes, with an immutable default and a separately declared bounded writable laboratory region. A valid ATR or successful synchronous read says nothing about issuer identity, application validity, available balance or billing. A simulated monetary account belongs to the separate test host ledger, never to a contact-test fixture.

## Historical application inventory

The late universal graphical build enables the original Gemplus GPM103 stored-value application and handler. The underlying async protocol stack is present, but the other payment applications are disabled in that particular build. The following families were found in the configurable firmware. Their inclusion does not establish that a simultaneous multi-application build will compile, fit RAM, or accept a card.

| Family | Transport/application evidence | Qualification direction |
| --- | --- | --- |
| Original GPM103 | Synchronous memory-based disposable purse; matching handler enabled in current build. | Existing contact fixture proves physical reads only; use authorized reference cards for actual historical application acceptance. |
| GPM416 | Separate synchronous memory-card family with an optional handler; not linked in the inspected baseline. | Confirm the corresponding handler and physical protocol in a separate build. |
| Smart City | Optional application with GPM416, PCOS and MPCOS selections; PCOS/MPCOS are reloadable processor-card families. | Establish selected transport and legitimate app compatibility independently; keep module sessions isolated. |
| Proton | Optional processor-card purse; separate large-display applet/UI integration. | Test native ATR and T=0/T=1 first, then authorized card/UI interoperability on a distinct build. |
| Mondex | Optional purse and session/user-interface features, including historical ADSI integration. | Native transport, applet interaction and network service are separate gates; ADSI does not require Mondex to be enabled. |
| Visa stored-value options | Separate disposable/reloadable product selections and application handling. | Card type alone is not an established transport or acceptance profile; validate with authorized documentation and cards. |
| Card Touch | Optional application with user-card and module integration. | Establish chip and protocol from an authorized card/reference, not inference from the application label. |
| Generic rechargeable handler | Placeholder dispatch entries found; no complete generic implementation established. | Do not claim support merely from an available configuration flag. |
| SAM / internal security modules | Async contact endpoints distinct from the inserted customer card; multiple physical selections. Specific module models are not established. | Qualify slot routing/reset/ATR isolation with synthetic non-payment responses; authorized application fixtures are separate. |

Use a plug-in provider per application family, with explicit capability, supported transport, command/response interface, error status and reset/removal lifecycle. The generic card model must not infer a valid application from ATR alone. This handoff intentionally does not include historical issuer recognition byte maps, purse programming layouts, credentials, key material or authentication/debit emulation recipes. Those require authorized application specifications and genuine or purpose-issued test cards; none are necessary to qualify native contact transport. Synthetic application data should have its own namespace, immutable lab file contents, and an independent test ledger. It must not impersonate an issued payment product.

## Native acceptance plan

1. Qualify the existing sync-only build with the nonfunded contact fixture, repeat insertion/removal, and demonstrate expected normal-call rejection. Preserve the earlier and later board behavior.
2. In a separate async-test build, exercise the maintenance test with valid T=0 and T=1 ATR vectors, missing/invalid ATR and inverse-convention tests. Record physical selection, power, reset, clock, serial bytes/parity and actual firmware outcome. Do not expect an async test in a firmware specifically configured for the sync maintenance test.
3. Use a native contact test harness to exercise the synthetic application above via T=0 and T=1; demonstrate that changing UART parameters, invalid commands, parity/EDC failures, timeouts, and mid-transaction removal behave correctly.
4. Alternate user contact and each internal module fixture. Verify that no UART byte crosses a selected endpoint boundary and that coin, NCC and normal user I/O remain unchanged.
5. Exercise separate authorized application builds with legitimate test cards. Log contact transport, application recognition, authorization, connected call, terminal call record and settlement independently. An approved lab quote without a native call record or charge is not a payment pass.

## Standards references

- [ISO/IEC 7816-3:2006, electrical interface and transmission protocols](https://www.iso.org/standard/38770.html), for general contact activation and asynchronous transport terminology. The original board design predates this edition; validate exact legacy behavior on the actual device.
- [ISO/IEC 7816-4:2020, commands for interchange](https://www.iso.org/standard/77180.html), for generic command-response and status semantics, not proof of a historical purse's proprietary application protocol.
