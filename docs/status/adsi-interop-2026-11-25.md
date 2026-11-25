# Graphical voice-service interoperability

## Implemented paths

- The central office can select exact local voice aliases ahead of normal
  SIP routing. The service receives a loopback UDP INVITE without REGISTER,
  with a reserved local RTP receive socket and PCMU at 8 kHz. Failure of a
  configured local route cannot reach an external destination.
- The analogue input to the graphical terminal drives the CAS dual-tone
  detector and the Bell 202 frequency discriminator. Carrier and CAS status
  are visible on the graphical expansion and SSI modem interfaces. Decoded
  serial bytes are delivered to the CPU's physical ASCI1 receive pin, using
  its programmed serial format. The NCC byte path stays independent.
- Genuine firmware outpulse events are the only source of post-answer DTMF
  on the local SIP route, including the extended A through D keys. No screen,
  acknowledgement or server message is synthesized for the terminal.

## Component evidence

- The local route, no-fallback behavior, manager relay handoff and DTMF
  waveform have CO regression coverage. Fourteen CO tests passed.
- The native receiver test covered CAS and single-tone rejection, Bell 202
  8N1 decoding of the first test frame and additional bytes across 24 phase
  and noise cases, disabled demodulation and carrier release; it passed.
  The SSI detect-register test and contact-card regression also passed.
- The actual dedicated local SIP service returned 100/200 to a standalone
  CO-side component call. The bridge received nonzero PCM and sent BYE on
  the established dialog. This is transport evidence only, not acceptance
  by the graphical firmware.

Live graphical firmware CAS, terminal acknowledgements, menu rendering,
interaction and disconnect supervision are separate acceptance gates.

## Live graphical bench

- The installed graphical firmware entered Craft, selected item 369 and
  initiated a normal force-download. Its isolated host recorded 18 of 18
  application acknowledgements, including all three large graphical records;
  only the screening record was revised to permit the local free route.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T070641.527238Z/`.
- Cabinet closure did not restore service in that process. A normal exit and
  restart with its firmware-written battery state brought back the service
  and dial prompt. The phone subsequently returned out of service, with an
  acknowledged telephony-board alarm, and did not outpulse the dialed alias.
  No SIP INVITE, CAS or terminal ACK was observed for this attempt. A targeted
  TP keypad/command and host-maintenance diagnostic is required.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T072747.194426Z/`.
- The focused TP trace recorded repeated link initialization attempts and a
  complete 126-byte configuration message. Its computed and transmitted
  checksums were both `0xC0`. The original behavioral TP program and its
  serial wrapper both interpreted that in-frame value as a fresh header,
  leaving the response and subsequent keypad events blocked. Fixed both
  parsers to consume every declared frame octet as data, and invoke the TP
  configuration checksum validation before acknowledgment. Regression tests
  cover valid `0xFF`, embedded `0xC0` and relay opcodes, a `0xC0` checksum,
  and a deliberately corrupted checksum. The assembled controller and
  native C++ test pass. This is not a terminal memory or table change.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T073741.163590Z/`.
