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
