# Native contact transport and card application gates

## Implemented hardware

The later motherboard now routes the customer contact to the actual Z180
ASCI0 pins only when the asynchronous gate, customer selector, supply latch
and generated clock are selected. The existing earlier-board coin ASCI0
and later-board external coin UART remain independent. The other selectable
module positions do not receive or emit customer-card bytes.

The virtual card begins its ATR only after its configured internal or external
reset activation. Direct and inverse convention have distinct serial bit
conversion and parity. ATR parsing checks interface byte presence, historical
length and TCK when required. A deliberately invalid ATR is transmitted for
negative testing but cannot enable application commands. Card timing comes
from its independent clock and an allocated emulated serial timer, not a
terminal status shortcut or a host-wall-time loop.

T=0 transfers actual procedure characters and separate command/response data.
T=1 handles LRC, I/R blocks, sequence numbers, incoming and outgoing chaining,
retransmission, IFSD negotiation, resynchronization and abort. The lab profile
supports a bounded PPS exchange and changes the card clock division after
its final response character. Unsupported proprietary instructions fail.

The supplied lab application selects an invented read-only object, by an
invented application identifier or synthetic file ID. Its response is an
immutable test string, not an issued payment product or a funded balance.
Transport and application acceptance remain separate.

## Native synthetic CPU verification

| Probe | Observed |
| --- | --- |
| Direct T=0 | ATR, procedure character, application selection, memory and status read through ASCI0 |
| Direct T=1 | ATR, checked selection, IFSD negotiation, chained memory response and R-block acknowledgement |
| Inverse T=0 | Inverted bit convention and odd physical parity, same read-only application exchange |
| PPS F=372, D=2 | Complete PPS exchange at initial rate and successful subsequent T=0 transaction at nominal 19,200 baud, with explicitly configured 1,500 microsecond additional device spacing |
| Synchronous memory | Timed test-region programming, removal/reinsert, independently restarted emulator and unchanged neighbouring bit |

Native card runs: `build/firmware-acceptance/smartcard-async-probe/20261125T182357.718171Z/`
and `build/firmware-acceptance/smartcard-pin-probe/20261125T182402.283326Z/`.
The writable synchronous test card stores its bounded one-way memory separately
from the terminal battery, keyed to its immutable fixture digest. Each terminal
profile retains one configured writable fixture state, not a collection of
independent card balances. Read-only fixtures do not write card state. No CPU
RAM, install flags or firmware are edited by these probes.

A faster PPS control with zero additional spacing emitted the complete
correct synthetic response, but the native terminal UART missed characters
during the response. It remains an unqualified timing condition. Evidence:
`build/firmware-acceptance/smartcard-async-probe/20261125T182419.854973Z/`.
The tested PPS variant uses the explicit synthetic spacing; it does not claim
arbitrary F/D rates or all card guard and waiting-time parameters.

Component tests also cover malformed ATR/LRC, pending response cancelled by
selection/removal, rejected writes and out-of-range reads, and bounded card
memory restoration. General background: [NXP AN4453](https://www.nxp.com/docs/en/application-note/AN4453.pdf),
[ISO/IEC 7816-3](https://www.iso.org/standard/38770.html) and
[ISO/IEC 7816-4](https://www.iso.org/standard/77180.html).

## Historical application gates

The reference MTR 2.12 standard and graphical builds select a GPM103-style
synchronous disposable card application. The GPM416 and Proton applications
require independently enabled firmware builds. The current catalogue identifies
their device and transaction families, but does not provide a specific non-funded
reference memory geometry, operation timing or issuer test application vectors.
An authorized reference card, nonsecret contact and operation evidence, and
corresponding test scheme are needed before native recognition can be claimed.
GPM103 is the first priority; GPM416 and Proton remain separate qualification
targets. The standard two-line MTR 2.12 and graphical build are the initial
GPM103 targets; the older packaged MTR 1.9 needs an independent application
qualification. No balance or payment acceptance is inferred from this transport.
The [Gemplus GemCore reference](https://ifd-gempc.apdu.fr/e5221031.pdf)
identifies GPM103 as a distinct memory-card driver, and the [GCI400 reference](https://docs.rs-online.com/e4e8/0900766b800292a9.pdf)
explains that the reader translates command envelopes into memory-card timing
sequences. Those host-reader interfaces do not establish the exact contact
timing of the terminal's reference card.

## Windows window size

The native window probe measured the default client at 376x886 pixels,
the explicit 2x bound at 720x1696, and unrestricted maximize at 880x2073.
The original artwork aspect ratio and monitor work area constrain the bounds.
The two launchers accept `--scale 2`, `--maximize`, or an independent INI
window preference. Run: `build/firmware-acceptance/window-scale/20261125T181157.279250Z/`.
