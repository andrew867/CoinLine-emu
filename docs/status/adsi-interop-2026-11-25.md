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
- The rebuilt native terminal accepted the real configuration, received TP
  status, cleared the telephony fault, and recognized physical keypad digits.
  It outpulsed the isolated free route, established a SIP dialog and received
  actual PCMU over RTP. The audio detector asserted CAS for 80 emulated ms.
  A passive service-side capture independently matched the transmitted
  85 ms CAS waveform. No A acknowledgment appeared on the wire, and no FSK
  frame followed. The service entered its voice fallback, and physical
  hangup sent BYE with a 200 response. The menu is not yet qualified.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T074245.460293Z/`.
- The subsequent status trace proved that the CPU read the asserted CAS
  input 32 times at 2.5 ms intervals. The graphical hardware requires a
  24.576 MHz oscillator (12.288 MHz effective processor clock), not the
  12.288 MHz oscillator of the earlier board. With the corrected profile,
  the unchanged tone produced 64 reads at 1.25 ms and actual firmware SSI
  DTMF A. The service received it, but the first response exceeded the
  capability window. No CAS stretching or service timing change was made.
  Runs: `build/firmware-acceptance/graphic-firmware/20261125T074808.455378Z/`
  and `build/firmware-acceptance/graphic-firmware/20261125T075338.975781Z/`.
- The CO now waits for the direct SIP receive worker before sending its
  first INVITE and runs polarity transitions without pausing the audio
  clock. The next actual call had prompt SIP ACK and a 540 ms measured
  CAS-end to A-onset packet interval, compared with 707 ms before these
  changes; it still missed the service's 500 ms window. The next transport
  revision clocks direct PCMU on the negotiated socket rather than staging
  it through a second packet-manager queue. Loopback media tests cover
  both directions, packet parsing and bounded receive buffering.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T080055.144411Z/`.
- The direct RTP socket revision passed a two-direction UDP regression and
  reduced the independently captured A onset interval to 424 ms. The
  completed digit still arrived at 523 ms, just outside the service window.
  The service sent voice fallback, not FSK; apparent receiver bytes during
  that audio were false detections. Tightened acquisition to require a
  sustained coherent mark and disabled TCP packet coalescing on both ends of
  the real-time line-jack connection. Both changes preserve the actual
  CAS and tone durations.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T080755.639305Z/`.
- A further call exhibited 20 ms receive gaps in a nominally continuous
  CAS. The independent capture verified all 680 tone samples in contiguous
  RTP sequence order, with one 54.796 ms packet-arrival interval. Removed
  independent zero-padding of direct receive audio, deliver actual complete
  frames to the jack, and added a two-frame (40 ms) native playout cushion.
  The direct DTMF waveform now follows the terminal's 60 ms tone and
  interdigit cadence. The V1.3 control-board clock selection also applies
  to its conventional display profile. The updated native executable and
  18 CO regression tests pass; live end-to-end requalification is underway.
  Run: `build/firmware-acceptance/graphic-firmware/20261125T081348.677583Z/`.
