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

## Dial-tone and telephony startup regression

The later relay correction exposed an assumption in the audio mixer: it
could only supply dial tone from the CO when the external loop was seized.
At pickup the terminal selects supplementary handset power and requests
local tone using Voiceware bank zero, sample 0x12. In this state the real CO
loop remains released. The prior build emitted silence on both recorded
channels during that request, even while the terminal accepted keypad input.

The handset now selects a local nominal 350/440 Hz sinusoidal source when a
firmware dial-tone request cannot be supplied by the connected CO receive
path. Its gain is independent of the enclosure volume latch. A genuine first
digit, other voice phrase, voice reset or hook replacement cancels the request.
This does not re-seize the external line or send a synthetic keypad event.

The graphical startup trace also showed a power-on token before the modeled
control-board serial link was enabled. The corrected controller reset is
coupled to power-fail release, samples the actual hook level and starts its
own clock epoch. CP-cycle conversion now accounts for the Z180 divide-by-two
clock interface. At the ten-second graphical snapshot, the former telephony
board warning has been replaced by the normal in-service prompt. The native
control processor consumes checksum-valid C4 and C0 frames in the initial
handshake rather than relying on subsequent maintenance to clear the alarm.

The failed baseline capture is under
`build/firmware-acceptance/dialtone/20261125T191846.584909Z/`. The native
acceptance probe measures both channels of the real emulator recording;
the capture is private run evidence, not a distribution asset.

Final native binary: both installed firmware profiles passed the boot/status
handshake and the real handset recording gate. Each produced fifteen
consecutive qualifying 200 ms windows of 350/440 Hz tone before the first
digit and zero afterward. Both profiles used independent copies of existing
installed storage; no install flags or memory were patched. The final runs
are `build/firmware-acceptance/dialtone/20261125T193331.470132Z/` (graphical)
and `build/firmware-acceptance/dialtone/20261125T193403.241994Z/` (OEM).

A separate cold OEM run on the final binary completed the CO and answer
supervision steps, reached the NCC download prompt and made no external SIP
INVITE. Evidence: `build/firmware-acceptance/oem-answer/20261125T193433.728971Z/`.

The native graphical New Call check also repeated successfully: first SIP
answer, actual New Call key, BYE, and second SIP answer while the hook
remained lifted. Evidence: `build/firmware-acceptance/new-call/20261125T193526.929649Z/`.
The component suite passed 65 C++ tests with two optional ROM-dependent
skips, and 50 Python tests with four subtests.

The final binary also emitted all four connected keypad events `25*#` over
RFC 4733, with the expected marked starts, 720-sample durations and three
end packets each. A shutdown speed banner interleaved the final native
diagnostic line; removing that exact banner recovers the complete four
hardware-gated events without changing the wire evidence. Run:
`build/firmware-acceptance/connected-dtmf-sip-native/20261125T193651.077382Z/`
(`result-reparsed.json`).
