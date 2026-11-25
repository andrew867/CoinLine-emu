# Remaining peripheral qualification and board variants

## Purpose

The large-display image now boots and renders received graphical advertising. Native voiceware playback, dual-language presentation, ordinary and desktop profiles, synchronous card contact, and the early ADSI audio/data dialogue have all been demonstrated to varying degrees. The following inventory identifies the next useful fidelity checks without assuming that an existing high-level model or successful component test qualifies the physical peripheral.

## Incoming ring and enclosure speaker

The later board has a separate ring-detector input at I/O read address `0x41`, mask `0x80`, set during detected ring. The same status byte includes DTMF data-valid (`0x20`), digit code (`0x0F`) and the telephony serial handshake (`0x40`); update each field independently. External ringing is permitted by the `0x40` write latch, bit 4 (`0x10`), and the enclosure transducer source is selected separately on the later board.

The native detector samples every 20 ms. Starting in idle or a ring-off interval, it increments the qualification count for active samples and decrements it for inactive samples, without falling below zero. Ten net active samples qualify ring-on (nominal 200 ms with uninterrupted detection). Six consecutive inactive samples qualify ring-off (nominal 120 ms). A five-second abandonment interval distinguishes the end of an incoming call from an ordinary quiet part of a ring cadence. These are software observations, not a specification for the analogue ring detector's voltage threshold or frequency response.

Acceptance: feed actual line ring intervals into the detector, verify native sampling, first valid ring, inter-ring pause, abandoned ring, answering and restoration of idle. Assert that the handset hook remains unchanged until the handset actually moves. Test ring during/after the reminder alarm and during a data-jack call. Measure the enclosure output separately from handset audio. See [forgotten-card alarm](forgotten-card-alarm-black-box-handoff.md).

## Telephony and modem ownership

The hardware supports independent handset state, line loop/supervision, internal modem isolation, external data-jack loop, and the telephony processor's serial command link. A modem management session releasing its relay is not a handset-release event. A connected local voice call is not evidence of payment collection or CDR creation. Live qualification has already exposed these distinctions.

Preserve the normal serial controller's framing, declared frame length and checksum even when an in-frame byte resembles a header. Verify response framing and input events after a complete configuration transfer. Use controlled missed responses and recovery to exercise the actual peripheral health reporting, not fabricated status flags. Keep analogue CAS/FSK, management data and dialpad/modem DTMF on their respective physical paths. See [ADSI timing](adsi-physical-timing-handoff.md) and [external data jack](data-jack-black-box-handoff.md).

## Multiple internal contact-module sockets

The revised board contains three distinct internal asynchronous module selector positions in addition to the customer card. The hardware selection, per-slot clock, reset, supply and async-resource ownership are independent. This is valuable to emulate even with harmless synthetic module responses: it tests actual multiplexing and separate session state, particularly when a customer card is removed while an internal module is idle or active. Do not infer the actual security-module part number or application from a selector. See [contact smart-card hardware](smartcard-contact-black-box-handoff.md).

## Power, persistent memory and board supervision

The board has a software-visible power-failure indication toward the telephony circuitry on the `0x40` write latch, mask `0x20`, and a periodic board sanity/watchdog output on the later board's `0x63` latch, mask `0x80`. The exact external watchdog timeout and analogue supply ride-through behavior require a hardware reference. Persistent EEPROM and battery-backed RAM have distinct responsibilities from volatile display and modem state. Do not assume that persisting the entire emulator process is equivalent to physical warm restart.

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
| Enclosure and cash-box status | Distinct physical switches produce distinct status and maintenance reports. Opening the cover is not itself evidence of a local siren; the normal reminder speaker is separate. Exercise ordinary authorized opening/closing and reporting without altering alarm policy or bypassing access controls. |

## Suggested order

1. Close the ADSI first-response timing gate and menu/softkey round trip.
2. Validate the enclosure output and incoming-ring interaction from actual registers.
3. Implement async contact transport and slot multiplexing alongside the qualified synchronous contact path.
4. Implement the external data jack with a local modem endpoint and compare its monitor/relay behavior with the desktop hardware.
5. Qualify cold/warm power and peripheral recovery in both controller profiles.

Each milestone should include component and native-firmware evidence. A component model may pass while the CPU-visible hardware path remains incomplete. Report those outcomes separately rather than marking a subsystem fully supported solely because a test helper exists.
