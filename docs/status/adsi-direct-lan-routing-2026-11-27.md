# Exact InfoLine extension routing

The portable CO now supports a collection of exact voice-service aliases,
each mapped to its own SIP extension. The previous single-target loopback
configuration remains accepted. A shared, optional `adsi-routes.ini` file
replaces only the ADSI settings for both portable terminal profiles; it
does not change their carrier sign-in, hardware settings or retained memory.

The dedicated service endpoint can be a loopback or private IPv4 address.
The direct UA selects the local interface for that endpoint, binds an
ephemeral UDP signaling port and reserves the actual RTP receive socket
before the INVITE. It does not REGISTER or use carrier credentials. Direct
media offers static PCMU/8000 and conveys ADSI/keypad DTMF in-band. The
configured host must also be the SIP source, Contact and SDP media host;
arbitrary public addresses, route sets and alternate media destinations
are rejected. A selected service route is consumed even on failure and
cannot fall through to ordinary SIP/PSTN routing. The numeric manager
destination is reserved for NCC and cannot be a service alias.

## Lab route contract

The optional shared configuration example maps the following extensions.
Long aliases use fictional NANP test numbers; their eleven-digit forms
are the same numbers with a leading 1. Terminal screening and numbering
plan support must be verified separately from routing at the central office.

| Extension | Ten-digit alias |
| --- | --- |
| 2374 | 7095550174 |
| 6500 | 7095550160 |
| 6501 | 7095550161 |
| 6502 | 7095550162 |
| 6503 | 7095550163 |
| 6505 | 7095550165 |
| 6506 | 7095550166 |
| 6507 | 7095550167 |
| 6508 | 7095550168 |

The unavailable extension 6504 is not in this list. The service and
manager must agree on the exact aliases; free-call rules must not be broad
prefix matches that inadvertently exempt ordinary toll calls. A permitted
short extension can be used directly; a number-plan restriction can be
handled with its authorized ten- or eleven-digit alias instead.

## Component evidence

- Parser and launcher tests cover legacy single-target and multiple-target
  routes, distinct aliases, duplicate rejection, private endpoint validation,
  the reserved NCC destination, and preservation of profile SIP settings.
- CO tests exercise exact route selection in both local and ordinary SIP
  calling modes, switch targets after hangup, and verify failure cannot
  fall back to a carrier destination.
- A real loopback SIP dialog verified an unauthenticated direct INVITE for
  a second extension, PCMU-only SDP, ACK, inbound and outbound RTP, in-band
  A, and BYE. The Windows suite passed 31 CO tests and 49 portable/fixture
  tests.

These checks establish the emulator and CO routing component. A local
distribution may explicitly bundle a service-only shared configuration;
the public package contains only the example, never site credentials.

## Live LAN and manager evidence

- The dedicated service gateway accepted the configured private SIP route.
  Separate CO and gateway tests verified actual PCMU traffic and the initial
  ADSI capability and CONNECT exchange.
- Both terminal profiles received published manager assignments containing
  exact free rules for all 27 dial forms. All 27 host quotes were free;
  ten control quotes, including paid and excluded destinations, remained
  unchanged. The standard profile has an MT92 update pending and the
  graphical profile initially had MT62 and MT92 pending. The manager later
  verified both selected graphical record acknowledgments; the standard
  profile was still pending at that observation. These checks do not by
  themselves prove the terminal's call behavior.
- A previously installed graphical terminal outpulsed the ten-digit InfoLine
  alias and reached the new LAN service in two separate native calls. Each
  produced SIP 200 OK, a real CAS/A exchange and an accepted CONNECT D1.
  The gateway independently decoded each complete 211-byte welcome frame,
  but the native PCM path experienced underruns during reception and did
  not acknowledge the welcome. This occurred with both the graphical UI
  (87.33% average emulation speed) and the headless run (99.97%). No server
  deadline was changed. Full interactive menu acceptance remains pending.
- An older installed configuration left the four-digit alias incomplete
  and asked for payment on the long alias. New manager assignments must be
  downloaded normally.

## Dedicated native rating and short-code check

A separate graphical terminal started from virgin storage, downloaded the
updated configuration through NCC and emitted 43 CRC-valid MT14 table
acknowledgments, including MT62 and MT92. It displayed `Completed 02` and
returned to `Please lift receiver` after the normal installation procedure.
The optional synchronous contact fixture did not pass the generic card
test, which was continued through the normal menu; coin calibration was
retried and the escrow test completed. This was a rating/routing acceptance
run, not a comprehensive card or coin hardware qualification.

With the new tables installed, the terminal dialed both the ten-digit
InfoLine alias and four-digit `2374` without a payment prompt. A retained
storage restart using the packaged SIP runtime then dialed `2374` and `6501`
as two successive calls. Both selected the corresponding distinct direct
SIP target, received SIP 200 OK, exchanged actual RTP, and released with BYE.
This establishes native short-code completion for those two extensions on
the tested graphical configuration. The remaining extensions have separate
gateway and host-quote evidence but have not all been dialed by firmware.

The ordinary paid test destination nevertheless outpulsed without a
payment prompt. The isolated CO did not answer or forward that destination.
This fails the paid-call regression control and was referred to the manager
for further isolated diagnosis; a successful free route does not establish
that ordinary calls are charged correctly. The original terminal's retained
storage was not used or modified for these runs.

Routing acceptance, host quote acceptance, newly installed rate-table
behavior and interactive ADSI playback are distinct gates.
