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
