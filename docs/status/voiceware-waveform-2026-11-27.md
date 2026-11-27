# Native uPD7759 waveform playback

The stand-alone uPD7759 supports both ADPCM speech and PCM waveform elements.
The [manufacturer data sheet](https://pdf.dzsc.com/D77/UPD7759.pdf) identifies
both synthesis methods. The observed banked directory distinguishes ADPCM
records (`0x00`) and waveform records (`0x40`). The previous implementation
rejected the latter before the device could start playback.

The MAME sound core now has an explicitly enabled stand-alone waveform
state machine. It reads the addressed ROM window, advances the DAC in
native 640 kHz clock units, repeats elements, interprets the observed
control and trailing pause, and returns BUSY to idle through the same path
as ADPCM. Reset cancels either mode. The bank, record boundary, duration
budget and malformed opcodes are checked; a message cannot consume the
following record or wrap the bank. The parser is shared with the driver
lookup and standalone synthetic tests. Other MAME configurations and slave
mode remain unchanged.

For the validated record length, the driver first checks BUSY near the
earliest possible completion, rather than every 10 us throughout a long
tone. Subsequent checks observe the actual device edge and raise INT0.
Reset and a bank change still cancel or reschedule the pending observation.
Local dial-tone requests accepted by the native waveform path no longer
also enable the synthetic dial tone or mix duplicate CO dial tone. The
independent telephony-controller DTMF and CO line media path remain intact;
the new waveform decoder does not manufacture or transmit keypad digits.

## Native I/O and PCM qualification

An isolated synthetic CPU fixture issued each of the 38 waveform sample
requests through the actual board ports, independently checked BUSY and
completion INT0, and recorded the native emulated output. The microphone
was not captured and no external call was made. A second dial-tone request
was reset during playback and correctly went idle without a completion
interrupt. All 38 full records completed exactly once. Durations measured
from the native start to INT0 include the chip startup interval:

| Sample | Purpose | Native completion |
| --- | --- | ---: |
| `0x12` | Dial tone | 20,429.26 ms |
| `0x13` | Bong | 979.06 ms |
| `0x14` | Attention | 550.65 ms |
| `0x18` | First DTMF waveform with trailing pause | 156.01 ms |
| `0x31` | Feedback | 101.71 ms |

The measured output contained the expected approximately 350.9 and
438.6 Hz dial-tone components, successive 914.3, 1367.5 and 1818.2 Hz
attention-tone components, and all 16 distinct DTMF pairs in the first
DTMF sample group. All 38 records produced non-silent PCM. The bong's
rendered envelope decreased in amplitude as its controls changed. Each
observed completion was within 1 ms of the independently calculated record
duration (including the normal startup interval).

The native probe is `tools/acceptance/voiceware_waveform_probe.py`;
aggregate evidence is retained locally in
`build/firmware-acceptance/voiceware-waveform/20261127T000707.087518Z/`.
The full C++ suite passed 71 tests, with three optional external-input
tests skipped; all 48 Python fixture tests passed.

## Installed terminal regression

Both the two-line and graphical installed references were tested using
independent copies of their retained battery data. Each resumed service,
completed the native telephony startup handshake, requested sample `0x12`
through the normal firmware path on pickup and produced the actual dial
tone. Each had 20 qualified audio windows before the first keypad digit
and none after it; ordinary ADPCM prompt playback was also accepted twice
on each reference. Both runs ended on hook without an external call.
The local test number reached a payment prompt rather than a denial, so
it is not evidence of a rejected-number announcement sequence.

An additional retained graphical reference requested the attention tone
from real terminal execution during a normal restart/maintenance screen.
The native sample started and produced completion INT0 550.65 ms later;
the telephone subsequently offered dial tone. This establishes that the
firmware-driven attention command now reaches the waveform engine. The
complete No access screen followed by its spoken announcement still
needs a separately configured blocked-number test.

## Physical qualification limits

The waveform length and clock-divider interpretation is supported by the
complete observed sample group and the measured spectral results. The
first point is interpreted as a control position with midpoint output,
the low control bits as provisional linear gain, and the observed trailing
pause as 48 speech-engine silence units. Absolute amplitude, exact DAC
mapping, the complete control encoding and pause units still require a
physical chip comparison. No fabricated waveform, recorded prompt or
display-triggered sound is used. The measurements above establish native
emulator operation, not physical chip parity for every format field.
