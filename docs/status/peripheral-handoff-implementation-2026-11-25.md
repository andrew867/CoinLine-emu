# Peripheral handoff implementation and qualification

## Enclosure warning source and audio

- Later-board port F now selects the enclosure source independently of
  its warning waveform and cadence gate. Only the selected, unmuted
  warning path produces audio. Card-supply latch edges with ring selected
  are silent, and a static output does not continue a synthesized oscillator.
  Enclosure warning remains independent of handset state and microphone
  transmission. Actual edge times and the audio stream share the emulated
  timebase rather than assuming oscillator frequency equals CPU cycles.
- The earlier Desk profile selects its documented nominal 3 kHz legacy
  oscillator with the active-low PC5 relay. This is a profile implementation;
  physical gain and revision-specific native reminder behavior are unqualified.
- A synthetic Z180 program wrote the actual later-board GPIO and recorded
  native mixed audio. The GPIO frequency was 698.340 Hz, and the selected
  warning output measured 697.457 Hz with RMS 2522 signed 16-bit units.
  The unselected, cadence-muted and restored-ring phases each had zero RMS.
  The measured frequency is derived from actual port timestamps and audio
  samples, not a hardcoded audio frequency. The complete probe repeated
  successfully after the data-jack changes.
  Run: `build/firmware-acceptance/enclosure-alarm-probe/20261125T142032.652560Z/`.
- The synchronous card contact regression now exercises shared port F
  warning transitions while a card is already powered and addressed.
  Neither the selected memory bit nor the address changes. The subsequent
  native two-selector contact-memory probe also passed.
  Run: `build/firmware-acceptance/smartcard-pin-probe/20261125T142030.742011Z/`.

Actual application-triggered forgotten-card delay, cancellation, ring
interaction and physical acoustic calibration remain separate acceptance
gates; the synthetic CPU probe does not claim them.

The distinct short-warning cadence also passed a native synthetic CPU
probe: three selected bursts of approximately 30.057 ms, alternating
with three muted intervals of approximately 40.076 ms, then restoration
of the ring source. Actual GPIO frequency was about 1,000.977 Hz; audible
phases had nonzero RMS and all muted phases had zero RMS. Evidence:
`build/firmware-acceptance/enclosure-alarm-probe/20261126T022529.239370Z/`.
The ordinary 700 Hz source/gate probe passed again in
`20261126T022536.378037Z/`. These are physical output-path tests; they do
not fabricate a qualifying card or claim a native application-triggered
reminder occurred.

A native incoming-ring overlap probe held the local line-jack ring input
asserted during the warning-source selection sequence. The actual CPU
observed the ring detector and wrote the warning latch. Recorded enclosure
audio was approximately 582.87 Hz averaged over the two alternating ring
stages before selection, zero RMS with the warning muted, 697.42 Hz during
the selected warning, and 582.84 Hz after restoring the ring source. The
line jack reported on-hook and ringer enabled, with no off-hook transition.
Evidence: `build/firmware-acceptance/enclosure-alarm-probe/20261126T032938.161748Z/`.

An installed graphical reference then exercised a normal handset lift,
generic synchronous card insertion, handset replacement with the card
still present, and subsequent card removal. The native terminal displayed
`Please remove your card` after handset replacement and returned to
`Please lift receiver` after removal. Its captured port F traffic contained
only the ordinary `00`, `08` and `0A` latch values, with no warning-source
selection in this trial. This qualifies the real reminder display and
removal recovery, not an application-triggered audible enclosure warning.
Evidence: `build/firmware-acceptance/nla1x08-native-forgotten-card/20261126T054833.995978Z/`.
The ordinary and short-warning checks passed again. This qualifies source
arbitration with incoming ring energy; application-triggered reminder delay
and cancellation still require a qualifying native contact application.

## External data jack

- A later-board opt-in hardware monitor models external connector presence
  and the modem's loop seizure independently. The read of `0x40` overlays
  only bit 7; the write latch is unaffected. The three documented raw relay
  output profiles have separate decoders. An unverified fourth combination
  reports unknown rather than inventing a connection. Internal relays and
  DTMF attenuation are retained separately from the external relay mask.
- A native bus probe demonstrated disconnected, plugged-idle, seized and
  unplugged-while-seized conditions without changing the handset input or
  unrelated board status bits. It observed actual later PIO writes for all
  three documented downloaded-profile routes and the unknown combination.
  Run: `build/firmware-acceptance/data-jack-pin-probe/20261125T142028.840546Z/`.
- Legacy relay polarity is an explicit configuration rather than inferred
  from the display or a firmware version string. The ordinary release path
  leaves the data-jack monitor disabled until the physical profile is selected.

This qualifies the monitor and raw control decoding only. Analogue switching,
external DTMF detection and modem handshake/bidirectional data remain open.

The optional later-board profile can now be selected in the portable
launcher and graphical bench without inherited environment overrides.
An installed NLA1X08 run observed the native monitor read change on separate
plug, seize, release and unplug inputs; the office remained connected and
the handset remained on-hook. No data-call relay transition occurred after
the input changes, so the result is monitor observation, not a completed
data call. Evidence: `build/firmware-acceptance/datajack-installed-firmware/20261126T034508.291684Z/`.

### Connector and profile follow-up

J34's two inner harness positions connect to the external data-jack module;
its outer positions are joined by a current-limiting link. The numbered
connector orientation, polarity and analogue transfer function are not
established by that harness drawing. The model now tracks available office
line independently of plug presence and external modem seizure. The
documented historical non-download monitor polarity is low, while the
downloaded-code profile uses high. An explicit polarity override remains
available for a measured alternative profile. The fourth relay combination
still reports unknown rather than inventing an analogue topology.

Native bus probes passed all four raw output combinations and monitor
polarity for the downloaded, desktop and universal logical profiles on
the later-board bus. They also changed office availability independently
while the external loop remained seized and the handset remained unchanged.
Evidence: `build/firmware-acceptance/data-jack-pin-probe/20261125T231238.457814Z/`,
`20261125T231239.762020Z/`, and `20261125T231241.251498Z/`.
The legacy physical board wiring, current-detector threshold and actual
analogue path remain unqualified. Do not equate the existing switch-level
monitor abstraction with a measured current comparator.

## ADSI deterministic timing fixture

The fixture generator produces 8 kHz signed PCM with 1 second of initial
silence, an 85 ms CAS, a 320 ms interval, one valid Bell 202 CONNECT segment
and two seconds of real silence. The synthetic segment has 1,814 samples;
the entire fixture has 29,054 samples. The native 8 kHz scheduled receiver
does not consume from wall-clock RTP queues. It uses the same CAS/FSK decoder,
physical serial receiver and carrier path as the ordinary emulator.

The first new local CO run returned to service but its socket did not deliver
terminal control events to the CO. An isolated native jack regression later
passed against both the preceding release and current executable, receiving
the genuine initial H0, E0 and Q0 controls. This does not determine the exact
reason for the failed live control session. Runs:
`build/firmware-acceptance/line-jack-probe/20261125T142049.842822Z/` and
`20261125T142101.042899Z/`.

The next offline harness observed the real exact local outpulse and fed
all waveform samples through the emulated clock. CAS was asserted at sample
8080 and released at sample 8720, exactly 640 samples (80 ms) apart. The
terminal did not poll CAS or generate A. Its call had not entered the
required voice session. In that attempt, the physical answer was started
before the internal dialling relay released. The fixture was revised to
wait for the genuine release before applying answer polarity, rather than
forcing a terminal session or injecting an acknowledgement.
Run: `build/firmware-acceptance/graphic-firmware/20261125T142128.716297Z/`.

The revised offline run again observed the exact local outpulse, waited for
the genuine modem release, and applied the two physical reversal phases.
The CAS detector asserted at sample 8080 and released at 8720 as before;
the terminal still did not poll it or enter the ADSI voice session. This
rules out that particular early-answer ordering as the sole gate. A
separate control-only local CO path, with the same deterministic receiver
clock and waveform, provides a further discriminator.
Run: `build/firmware-acceptance/graphic-firmware/20261125T142635.055629Z/`.

With the normal local CO control socket retained, the deterministic waveform
bench reached the actual voice session. The terminal generated A, decoded
all eight CONNECT bytes without framing errors, and generated D1 with no
second FSK stimulus. Measured native time:

| Interval | Result |
| --- | ---: |
| CAS segment end to A command onset | 188.588 ms |
| FSK segment end to first D command onset | 791.587 ms |
| Exposed carrier release to first D command onset | 777.471 ms |

The FSK segment end is calculated from the timer start and the fixture's
exact sample boundary, using 8,000 samples/s. The D timing is commanded
onset, not completed digit detection at an external service. The read-only
state trace still selected the ordinary 50-tick first-mode branch; neither
the branch nor its timer was changed. Run:
`build/firmware-acceptance/graphic-firmware/20261125T142841.910494Z/`.

This evidence prompted the live graphical receiver to use a dedicated
emulated 8 kHz timer as well. It consumes actual line PCM and provides a
separate bounded audible-monitor queue to the sound renderer. CPU-visible
CAS/FSK state is no longer advanced in sound-stream batches. Missing
samples during active carrier are traceable; the ordinary network buffer
and service deadlines are unchanged. Live menu qualification remains open.

## Live retest with independent receiver clock

Two actual local SIP calls used the rebuilt native receiver and unchanged
service. The first detected CAS for 80 ms with 64 actual status polls and
generated A, but the independent wire capture measured completion at 558 ms
after CAS ended, 58 ms beyond the 500 ms service deadline. The service
correctly used voice fallback and a physical hangup closed the dialog.

The second call received A within the capability window (223 ms completed
digit), delivered the exact first CONNECT, and generated native D1. Native
carrier release-to-D command onset was 834.371 ms and commanded D duration
69.217 ms. The independent wire capture measured completed D at 1,243 ms
after CONNECT ended. Its retry had begun 24 ms before the initial D onset
on the wire, so the service closed rather than deliver an ambiguous menu.
No active-signal PCM underrun was reported by the opt-in trace. This run's
whole-terminal average speed was 74.85%, not a controlled pre/post benchmark;
it does not isolate scheduler cost from concurrent host workload or establish
the speed during any individual interval. The independent-clock change is
native and the FSK path is functional, but reliable real-time menu delivery
remains unqualified. The next timing investigation should measure display
ready polling, DMA and initial mode-transition latency alongside host pacing.
Run: `build/firmware-acceptance/graphic-firmware/20261125T143933.669281Z/`.

No service deadline, installed table, acknowledgement or terminal memory
was altered to disguise the result.

## Component validation

The native Windows build succeeded. CTest passed 64 tests, with two
ROM-dependent skips and no failures. All 18 central-office tests and all
18 fixture tests passed, including
deterministic PCM sample boundaries and unchanged captured CONNECT input.
Native line-jack, data-jack and synchronous contact probes also passed
again after the final receiver-clock build.

## Full graphical forgotten-card control, 2026-11-26

The qualified full graphical image started from its own installed storage
and displayed `Please lift receiver`. A generic customer contact fixture
was physically inserted off hook, and the terminal displayed `Card
validation in progress`. After replacing the handset, the terminal later
displayed `Please remove your card`. The opt-in contact trace confirmed
physical presence changed from 1 to 0 on removal. The reminder screenshot
63.6 emulated seconds after hangup and the screenshot 30.5 emulated seconds
after removal were byte-identical. No enclosure alerter GPIO transition
occurred during the card-present or removal interval.

This establishes the visual reminder on this exact image, but not audible
reminder activation or recovery after removal. The latter two remain open;
the previous graphical image's removal result is not transferable to this
one. Independent read-only inspection of the retained state confirmed
that the terminal itself saw both sensors clear, classified the card as
out, and returned the reader to idle; its validation state was still
pending. The configured alarm duration was nonzero. This separates the
physical removal path from the application/UI cleanup and does not prove
that this generic contact fixture is eligible for an audible alarm.
No application response, alarm output or service state was forced.
The contact presence trace is opt-in and otherwise leaves the device
behavior unchanged.

Run: `build/firmware-acceptance/nla1x09-native-forgotten-card-repeat/20261126T085756.573966Z/`.

### Independent stock-image removal controls

Three unchanged stock images were restarted from copies of their own
previously installed battery and flash state, with the same generic
read-only contact fixture as the full graphical run. Each used its
previously qualified board profile, two-line display and a disconnected
local NCC endpoint. No payment or external call was attempted.

| Image | Board | Card-present display | On-hook reminder | After removal |
| --- | --- | --- | --- | --- |
| NLA1X05 | V1.3 | Cannot read card | Please remove your card | Please lift receiver |
| NQA1X01 | V1.3 | Cannot read card | Please remove your card | Please lift receiver |
| NKA1X02 | V1.1 | Cannot read card | Please remove your card | Please lift receiver |

In each case, physical presence changed from 1 to 0, the reminder was
still visible 63.6 emulated seconds after hangup, and the screenshot 30.5
emulated seconds after removal matched the initial idle screenshot
byte-for-byte. All three processes exited normally. Two enclosure latch
writes after removal retained the normal `08` value in each case; no
audible warning-source activation was observed. The generic rejected card
does not qualify the actual audible reminder policy.

The full graphical image repeated the same failure with a bounded,
read-only task trace. Across separate short insertion and removal sample
windows, the router entered recurring recovery 18 and 19 times respectively,
completed its recovery notification and re-entered with alternating signals.
Neither the pending card-removal cleanup nor validation-clear path was
observed. The terminal's raw sensors were clear and reader idle after
removal, while validation remained pending. The final reminder screenshot
was byte-identical to its pre-removal reminder. The process exited normally.
This distinguishes a recurring recovery condition in this image from a
missing physical card-removal signal. The cause of the repeated recovery
is still under investigation; there was no forced state change.

Evidence under `build/firmware-acceptance/oem-card-controls/`:

- `NLA1X05-async/20261126T131511.574333Z/`
- `NQA1X01-async/20261126T131601.353426Z/`
- `NKA1X02-async/20261126T131916.227881Z/`
- `full-removal-watch/20261126T132021.270191Z/`

### Startup routing discriminator

A further warm control used the same full graphical image and its own
installed storage, without lifting the handset or inserting a customer
card. The display reached `Please lift receiver`, but an opt-in bounded
read-only trace captured the following task sequence:

| Emulated frame | Observed event |
| --- | --- |
| 129 | Service card subsystem and router initialization |
| 182 | Initial generic handler transmit |
| 211 | Router receive and response-destination lookup; first error log entry |
| 212 | Second destination lookup, rejected transmit, serious-error recovery and subsystem reinitialization |
| 219 | Another service initialization transmit |

The 15-second control captured 82 serious-error entries, 164 router task
entries, 164 response-destination lookups, and 410 error-log calls. The
retained circular log independently showed repeated no-command-flow,
invalid-destination and transmit-failure errors followed by recovery.
The dedicated reminder start, cancellation and cadence task entry points
were not observed. The entire condition is therefore present before card
use and is not a consequence of the card switch or reminder timer.

The upstream review identified incorrect index width in shared router
command and response relationship bookkeeping as a concrete defect
consistent with this observed sequence. The recommended correction is
limited to normal relationship bookkeeping and response routing; do not
alter the alarm timer, payment validation or physical input behavior to
conceal the failure. Acceptance requires an idle startup with no recurring
router errors, normal rejection and removal of an unsupported test card,
separate audible alarm qualification on an eligible reference fixture, and
cold/warm service regression. This remains a diagnosis, not a tested fix.

Evidence: `build/firmware-acceptance/oem-card-controls/router-startup-watch/20261126T134617.195210Z/`.
