# Interactive and audio milestone status

## Verified

- Both firmware profiles completed the CO and answer supervision checks through the line jack.
- Both passed handset and keypad prompts, Track 2 swipe, and the nickel, dime, and quarter checks, then displayed **Please lift receiver** after INSTALL and again on restart.
- Both played firmware-driven Voiceware with uPD7759 completion interrupts. The earlier-board bank latch and per-bank sample lookup are exercised by the OEM quote.
- The demo outpulsed the configured toll-free number and received National Directory Assistance audio through SignalWire. The OEM requested a live rate, displayed five cents for the first minute, and spoke the matching price. The handset cannot speak individual cents below the firmware's five-cent steps.

See the [firmware, network, and audio evidence](ncc-install-firmware-stx-handoff-2026-11-23.md) and the [Windows launch controls](../../RUNNING.md).

## New Call follow-up

The physical New Call key originally reached the telephony controller and
caused a firmware restart command, but the CO remained seized. The relay
model treated connection to the modem as equivalent to loop-current sink and
ignored transfer of the handset to the supplementary supply.

The corrected line state resolves all three physical relay drives. The
handset contributes loop current only when connected to the CO; the modem
does so only while its connection and current sink are both active.

An isolated native graphical firmware run passed: initial SIP INVITE/ACK,
genuine New Call keypad input, line release and SIP BYE, firmware display
**Dial your next call**, then a second SIP INVITE/ACK without replacing the
handset. The emulator exited normally. No forced software hangup, firmware
patch, or installation-state edit was used. Evidence:
`build/firmware-acceptance/new-call/20261125T183219.375986Z/`.
The SIP peer was local and no public telephone call was placed.

The same packaged binary also passed a cold OEM CO and answer-supervision
regression: both results 00, one local answer polarity reversal, and the
NCC download prompt reached without an external SIP INVITE. Evidence:
`build/firmware-acceptance/oem-answer/20261125T183617.917100Z/`.

Connected graphical keypad DTMF also passed on the same native binary.
The real firmware emitted `25*#`; the loopback SIP peer received four RFC
4733 events, each with a marked start, 720 samples of duration and three
end packets. Evidence:
`build/firmware-acceptance/connected-dtmf-sip-native/20261125T183802.282177Z/`.
