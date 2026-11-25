# Connected DTMF and live ADSI navigation qualification

## Fresh real-time baseline and graphical service

After the local service workloads were moved to another host, the existing
Windows OpenGL native window probe with real graphical firmware and audio
reported 97.97% overall speed. Its short idle, caption, resize and recovery
clock ratios were 93.2%, 96.4%, 99.5% and 96.2%. The native firmware remained
unmodified and the service deadlines were unchanged.

The next installed graphical firmware call used an independently managed
remote CO and no periodic or pre-call screenshots. It reported 98.65%
whole-run speed. The native receiver decoded CONNECT and the welcome frame;
the independent wire measurement established the following completed tones
at 20 ms packet resolution:

| Milestone | Observed result |
| --- | --- |
| CAS and capability A | Genuine terminal A within the capability window |
| First CONNECT D | 944 ms after the first FSK burst ended |
| CONNECT count 1 | 1,064 ms after burst end, within its separate interdigit window |
| Welcome D | 625 ms after welcome FSK burst ended |
| Welcome count 1 | 745 ms after welcome burst ended |
| Native VFD | Actual welcome title and menu choices rendered |
| Selection | Numeric key and selection-key stimulus did not emit a standalone digit |
| Release | Service input timeout, genuine release D1, clean SIP termination |

The welcome, numeric-key and selection-key snapshots were identical. The
wire capture contained no selection. This is not an acknowledgement retry
failure. The data-mode transition disables autonomous dial-pad tones; an
interactive voice-between-screens service profile was subsequently tested as
described below. The emulator does not bypass the firmware's input policy.

Native runs: `build/firmware-acceptance/window-drag-probe/20261125T154116.594258Z/`
and `build/firmware-acceptance/remote-adsi-realtime/20261125T154352.825188Z/`.
The scoped independent wire evidence remains outside public source control.

## Live native graphical navigation

The service's voice-between-screens profile preserves the ADSI session,
returns to voice mode after interactive displays and uses a new CAS/A
exchange before the next data transfer. No response deadlines were changed.
The native terminal ran at 98.72% overall speed across 296 seconds with sound
and artwork enabled. No firmware or persistent memory was patched.

| Transfer | Independently decoded frame | Native terminal behavior |
| --- | --- | --- |
| CONNECT | 8 bytes | Genuine D1, session opened |
| Welcome | 192 bytes | D1/B, menu and six softkey labels rendered |
| News | 182 bytes | Genuine keypad 1, fresh CAS/A, new menu, D1/B |
| Welcome | 192 bytes | Genuine keypad 0, fresh CAS/A, identical welcome, D1/B |
| Release | 9 bytes | Genuine physical softkey 6 emitted 0, fresh CAS/A, D1/B, clean SIP BYE |

Four CAS/A exchanges, five valid frames, five D1 acknowledgements and three
real selections were observed without retries or invalid frames. Native
VFD snapshots confirm both distinct screens and the return to welcome.
The CO acknowledged the remote BYE and interrupted/restored loop current.
The terminal was then placed on-hook and exited normally.

Native evidence: `build/firmware-acceptance/remote-adsi-voice/20261125T161518.485582Z/`.
The VFD screenshots and independent packet captures remain ignored local
artifacts. Free graphical navigation is qualified; paid ADSI service,
other firmware families and physical terminal measurements remain separate
acceptance gates.

## Connected-call DTMF hardware and CO

The controller's 0x2D/0x2E commands now explicitly disable/enable autonomous
dial-pad tone generation. Initial dial-pad makes remained locally audible
but did not produce duplicate outpulses while the gate was disabled. A
real connected-call trace established that a modem relay remains energized
while the controller explicitly enables the keypad tone generator. The
voice-loop gate therefore distinguishes active management data from an
ordinary monitored voice connection, rather than treating the relay as an
exclusive data selector. The handset must be off-hook and a voice loop
must be present before a genuine accepted make can drive the line.

The CO dispatches connected line-tone events independently of call routing.
For ordinary SIP it selects negotiated RFC 4733 (dynamic 8 kHz payload and
supported event subset), explicitly supported legacy SIP INFO, or in-band
G.711 PCM. An ordinary digit uses a nominal 90 ms tone; the established
direct ADSI in-band cadence remains 60 ms on/60 ms off. RTP events use the
audio stream's sequence and SSRC, constant onset timestamp, increasing
duration and three final event packets. No duplicate in-band tone is added
when sending an RTP event or INFO request. INFO uses the existing dialog
route and local CSeq with transaction retransmissions. A peer's `Allow: INFO`
alone does not establish DTMF body support.

The actual SIP loopback test completed REGISTER, INVITE and ACK, then
verified all three transport alternatives over real UDP sockets. This is
transport qualification, not a claim about an external carrier.

A subsequent installed graphical firmware run completed a real loopback SIP
REGISTER, INVITE, 200, ACK, RTP audio reception and answer supervision. Its
physical keypad entered `25*#` after the firmware enabled the controller
tone generator. The native trace recorded exactly those four line events,
and the independently bound SIP peer decoded these RFC 4733 results:

| Physical key | RTP event | Duration at 8 kHz | Final packets |
| --- | --- | --- | --- |
| 2 | 2 | 720 samples (90 ms) | 3 |
| 5 | 5 | 720 samples (90 ms) | 3 |
| * | 10 | 720 samples (90 ms) | 3 |
| # | 11 | 720 samples (90 ms) | 3 |

Each event had its initial marker and its own onset timestamp. The terminal
went on-hook, the CO sent BYE, and the peer acknowledged with 200. The
native process exited with status 0 at 97.59% average emulation speed over
121 seconds. The test peer intentionally supplies a continuous 440 Hz audio
signal; it cannot call an external destination.

Run: `build/firmware-acceptance/connected-dtmf-sip-native/20261125T162338.314808Z/`.
The native test is reproducible using `tools/acceptance/connected_dtmf_native_probe.py`.
Ordinary carrier behavior and bidirectional real-world media still need a
separate provider test.

References: [RFC 4733](https://www.rfc-editor.org/rfc/rfc4733.html) and
[RFC 6086](https://www.rfc-editor.org/rfc/rfc6086.html).
