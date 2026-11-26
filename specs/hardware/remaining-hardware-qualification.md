# Remaining peripheral qualification and board variants

## Purpose

The large-display image now boots and renders received graphical advertising. Native voiceware playback, dual-language presentation, ordinary and desktop profiles, synchronous card contact, and the early ADSI audio/data dialogue have all been demonstrated to varying degrees. The following inventory identifies the next useful fidelity checks without assuming that an existing high-level model or successful component test qualifies the physical peripheral.

## Incoming ring and enclosure speaker

The later board has a separate ring-detector input at I/O read address `0x41`, mask `0x80`, set during detected ring. The same status byte includes DTMF data-valid (`0x20`), digit code (`0x0F`) and the telephony serial handshake (`0x40`); update each field independently. External ringing is permitted by the `0x40` write latch, bit 4 (`0x10`), and the enclosure transducer source is selected separately on the later board.

The native detector samples every 20 ms. Starting in idle or a ring-off interval, it increments the qualification count for active samples and decrements it for inactive samples, without falling below zero. Ten net active samples qualify ring-on (nominal 200 ms with uninterrupted detection). Six consecutive inactive samples qualify ring-off (nominal 120 ms). A five-second abandonment interval distinguishes the end of an incoming call from an ordinary quiet part of a ring cadence. These are software observations, not a specification for the analogue ring detector's voltage threshold or frequency response.

Acceptance: feed actual line ring intervals into the detector, verify native sampling, first valid ring, inter-ring pause, abandoned ring, answering and restoration of idle. Assert that the handset hook remains unchanged until the handset actually moves. Test ring during/after the reminder alarm and during a data-jack call. Measure the enclosure output separately from handset audio. See [forgotten-card alarm](forgotten-card-alarm-black-box-handoff.md).

Native register-driven warning, cadence, and ring-source overlap probes pass.
An installed terminal also displayed the real forgotten-card prompt on
handset replacement and returned to service after removal. The audible
application-triggered alarm remains a separate qualification gate. See
[native peripheral evidence](../../docs/status/peripheral-handoff-implementation-2026-11-25.md).

## Telephony and modem ownership

The hardware supports independent handset state, line loop/supervision, internal modem isolation, external data-jack loop, and the telephony processor's serial command link. A modem management session releasing its relay is not a handset-release event. A connected local voice call is not evidence of payment collection or CDR creation. Live qualification has already exposed these distinctions.

Preserve the normal serial controller's framing, declared frame length and checksum even when an in-frame byte resembles a header. Verify response framing and input events after a complete configuration transfer. Use controlled missed responses and recovery to exercise the actual peripheral health reporting, not fabricated status flags. Keep analogue CAS/FSK, management data and dialpad/modem DTMF on their respective physical paths. See [ADSI timing](adsi-physical-timing-handoff.md) and [external data jack](data-jack-black-box-handoff.md).

## Multiple internal contact-module sockets

The revised board contains three distinct internal asynchronous module selector positions in addition to the customer card. Independent supply, reset, clock and ASCI0 routing passed native synthetic transport tests, including all three module positions and the separate customer contact. The corrected full graphical image now passes the generic customer-contact self-test, all three distinct native internal-module diagnostics, full cold installation and retained warm service. A one-populated negative control correctly rejected the two empty sockets. Scheme initialization and authorized application qualification remain separate; do not infer the actual security-module part number or application from a selector. See [contact smart-card hardware](smartcard-contact-black-box-handoff.md) and [full-image qualification](../../docs/status/proton-graphical-qualification-2026-11-25.md).

## Power, persistent memory and board supervision

The board has a software-visible power-failure indication toward the telephony circuitry on the `0x40` write latch, mask `0x20`, and a positive watchdog pulse on the later board's `0x63` latch, mask `0x80`. Firmware nominally services the watchdog every 500 ms with a documented one-second maximum gap; the physical threshold and reset fan-out are unmeasured. Passive and provisional CPU-only supervision probes now exist, with separate NMI and control-rail inputs. See [native supervision evidence](../../docs/status/board-supervision-2026-11-25.md) for actual startup timing differences and synthetic acceptance. Persistent EEPROM and battery-backed RAM have distinct responsibilities from volatile display and modem state. Do not assume that persisting the entire emulator process is equivalent to physical warm restart.

Useful acceptance cases:

- Confirm clean cold provisioning followed by a genuine warm restart using only firmware-written persistent state. Report mount/flush behavior separately from the terminal's configuration validity checks.
- Observe a normal supply interruption and restoration in an isolated fixture, including the terminal's actual power-failure indication and pending peripheral operations. Verify no unexpected persistent-memory mutation after power is lost; document any externally established hold-up interval rather than inventing one.
- Exercise interrupted and completed non-payment table updates using expendable test state. Recovery must follow genuine firmware behavior; do not prepopulate validity flags or silently discard retained diagnostic records.
- Observe the watchdog output at the correct CPU clock and test ordinary reset under a documented external profile. Keep warm and cold reset distinct. A watchdog timeout should be calibrated before adding a new default reset timer.

## Other profile differences and acceptance opportunities

| Peripheral or product feature | Current evidence and recommended next step |
| --- | --- |
| Direct large-display navigation keys | The large display has its own direct matrix scanner, distinct from the conventional keys serviced by the telephony processor. Test physical softkey, arrow, simultaneous and release behavior in graphical and ADSI contexts. See [large-display handoff](11-line-vfd-black-box-handoff.md). |
| Displayless and maintenance-only configurations | The June 1998 multi-pay replacing-parts guide P0883897, pp. 7-6 to 7-7, and card-based replacing-parts guide P0883899, pp. 6-7 to 6-8, confirm a real portable VFD connected to control-board J7. The coin-basic unit fits temporarily in the ordinary display recess; the Inmate unit is magnet-mounted. Exact module identity and electrical pinout remain unknown. |
| Desktop terminal | Its working firmware profile still needs dedicated frontend artwork and a physical reference capture of the data-jack relay board. Do not extrapolate coin mechanism or enclosure dimensions from a wall-mounted multipay terminal. |
| Voice ROM banks and language switching | Native playback and dual languages are already demonstrated. Retain real chip busy/completion behavior and test changing language during call setup, digit composition and reset with authorized matching ROMs. Historical additional-language builds do not establish availability of their voice ROMs. |
| Board clock profiles | The later controller and its conventional-display variant require the later oscillator rate. Display type, CPU clock, DMA timing and peripheral map are independent profile attributes, not one graphical boolean. Regression-test timing for both board families. |
| Modem and external service | Ordinary management data and ADSI share applicable modem resources. Exercise arbitration and teardown using separate native sessions. Avoid introducing direct protocol injection as a replacement for missing analog hardware. |
| Serial peripheral health | Native coin and telephony link health reporting, timeout, reset and restoration should be validated using genuine disconnect/reconnect conditions. Fault recovery must not masquerade as a successful valid transaction. |
| Enclosure and cashbox status | An actual local quarter-rated call produced a coin CDR, followed by native MT51 collection and MT38 replacement-box reports matching separately persisted physical contents; no duplicates were observed. Separate collector authentication, physical sensor calibration, and other board variants remain open. See [cashbox qualification](../../docs/status/coin-cashbox-qualification-2026-11-26.md). |
| Remote firmware upgrade | A native MT19/MT9 callback delivered MT94, and the scheduled terminal dialed the independent code-server route. The connection ended before carrier and application data; a later native disabled MT94 cancelled the job. Image transfer and interrupted recovery remain open. See [download qualification](../../docs/firmware-download-storage.md). |

## Suggested order

1. Repeat ADSI first-response timing and menu navigation under controlled workloads; a full free-service round trip already passed.
2. Qualify the native application-triggered audible reminder; component enclosure and ring-overlap probes already pass.
3. Qualify native card and module application behavior independently of the completed generic transport probes.
4. Implement the external data jack with a local modem endpoint and compare its monitor/relay behavior with actual circuitry; raw logical profiles passed.
5. Qualify code-server carrier, normal image transfer, actual firmware interruption and peripheral recovery, including distinct control and telephony power domains. Schedule pickup and separate dialing already passed; passive tracing and provisional synthetic reset tests also passed.

Each milestone should include component and native-firmware evidence. A component model may pass while the CPU-visible hardware path remains incomplete. Report those outcomes separately rather than marking a subsystem fully supported solely because a test helper exists.
