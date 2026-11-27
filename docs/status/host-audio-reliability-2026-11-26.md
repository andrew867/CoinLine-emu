# Host audio reliability and runtime hot-path audit

The changes below reduce work on the emulation thread and protect host audio
from short scheduling stalls. The terminal CPU clocks, peripheral clocks,
firmware decisions, serial timing and remote media deadlines are unchanged.

## Runtime audit

| Path reviewed | Finding and disposition |
| --- | --- |
| Main Z180 SRAM writes | Optional fault provenance was building a full register snapshot, formatted JSON and a last-writer map on every write even with no consumer. Now bypassed when the relevant trace sinks are absent. Stack and RAM sidecar probes also return before reading registers when inactive. |
| Z180 core, DMA, IRQ and MMU | Upstream CPU/ASCI verbose logging is disabled. Actual bus mapping, interrupt handling and cycle accounting are retained. No timing shortcuts. |
| PCD3349A/8048 and physical input | Native instruction execution and required input sampling are retained. Existing trace sinks are checked before writing. Driver polling now uses one allocated timer per ID instead of repeated anonymous timers; reset cancels and rearms the polling sequence without accumulating duplicate chains. |
| Voiceware, alerter and audio routing | Optional route/phrase/alerter JSON formatting is bypassed when no audio trace sink exists. Unchanged /RESET latch writes no longer schedule redundant synthesizer resets; the first level and actual edges are retained. ADPCM BUSY still updates the native core; the inactive software stream does not need an additional status-read update. |
| Graphical display | Retain the existing visible-page optical cache and OpenGL default. The launcher permits automatic skipping up to 11 rather than restricting OpenGL to four under pressure. Display frames may be shed, not the terminal CPU work. |
| Host speaker and microphone | Default host output buffer target increased from 120 to 240 ms, independently configurable. Windows emulator and CO processes use above-normal, not high or real-time, priority. WinMM now queues eight 10 ms capture buffers instead of three, retaining the existing bounded microphone PCM ring. |
| CO line-jack PCM | A 20 ms frame can fit in one bounded socket read rather than requiring three polls. Pending encoded media is bounded by the PCM queue; a stalled peer cannot cause arbitrary partial-line removal. EOF/hard socket errors clear physical line state and a new attach reports current controls, not stale ones. Unchanged line input bits are not rewritten repeatedly. |
| NCC/serial | The running modem does not retain the standalone fixture transcript; explicit requested traces remain available. The native byte stream and the previously qualified poll cadence are unchanged. |
| Battery-backed persistence | A periodic immutable device snapshot is handed to a background writer, with at most one write in flight. If the disk is busy, the main emulation thread does not wait. Failed replacements leave the preceding complete battery file intact and retry later. Normal exit waits and writes the latest actual state. Earlier-board overlays retain the exact file format; later boards do not scan the unused low overlay twice per save. |
| ADSI and analog sampling | The native 8 kHz detector clock, CAS/FSK discrimination, ADC/DAC timing and existing 40 ms real-time line-media cushion are retained. The larger speaker buffer is local output only; it cannot delay a protocol ACK or alter the received signal. |

## Measured CPU headroom

Alternating native headless, unthrottled runs used the same installed
graphical reference, independent copies of its saved state, the identical
17-emulated-second handset-input sequence, no periodic snapshots, no host
speaker output and diagnostics off. The management endpoint was isolated.
The only executable difference was the SRAM diagnostic guard.

| Build | Run 1 | Run 2 | Mean |
| --- | ---: | ---: | ---: |
| Baseline | 53.89% | 49.64% | 51.77% |
| Diagnostic guard | 109.70% | 112.54% | 111.12% |

The mean ratio was 2.15 times. These are measured unthrottled emulation
speed ratios, not audio continuity or a guarantee of performance on every
host workload. The host was concurrently running other applications.

Evidence: `build/host-audio-reliability-20261126/baseline-b8063723/`
and `build/host-audio-reliability-20261126/trace-guard-1ec74a80/`.

## Speaker stress qualification

An opt-in WASAPI counter reports actual output underflow events and samples
spent waiting for re-priming, excluding initial priming. No user audio is
recorded. With the completed performance build, the same installed test
state and 17-second input sequence were run using graphical OpenGL artwork
and actual Windows speaker output. The bench restricted itself and its
children to two logical CPUs and added one bounded periodic CPU worker
(25 ms busy target per 100 ms period on one of those CPUs). The selected
CPUs had measured initial host usages of 68.8% and 74.2%; the background
workload was not held constant. The old and new host settings were compared
on the same executable.

| Configuration | MAME speed | WASAPI underflows, two render streams |
| --- | ---: | ---: |
| 120 ms, max skip 4, normal priority | 68.34% | 34 / 35 |
| 240 ms, max skip 11, above-normal | 82.64% | 9 / 9 |
| Same new settings, no artificial CPU restriction or worker | 82.85% | 10 / 10 |

The improvement is measurable but cannot eliminate underruns during
sustained emulation below real time. No host buffer can generate missing
native samples. Extended audio and live remote voice/data protocol
qualification remain separate tests.

Evidence: `build/host-audio-reliability-20261126/old-settings/20261126T233435.423664Z/`
and `build/host-audio-reliability-20261126/new-settings/20261126T233508.461522Z/`.
The unrestricted repeat is in `build/host-audio-reliability-20261126/unconstrained/`.

The final graphical run's own saved storage was independently restarted and
returned to the idle Please lift receiver screen, without another install.
The earlier-board two-line reference also reached the same idle state on
two consecutive launches with its own saved battery. The native line-jack
attach probe passed. These establish retained-state
and initial line-control compatibility, not a new remote call qualification.
The Windows native build passed 71 of 74 registered CTest checks, with three
optional input-dependent checks skipped, and all 48 Python fixture checks.

## Remaining Voiceware tone qualification

Subsequent implementation and native qualification are recorded in
[native waveform playback](voiceware-waveform-2026-11-27.md). The following
describes the gap as it existed during the runtime audit.

The terminal's No access path requests the attention tone at bank 0 sample
`0x14`, then proceeds to the normal announcement. This is not a generated
modem SIT or a VFD-dependent sound. The sample is one of 38 identified
waveform-mode (`0x40`) entries in bank 0, distinct from the supported ADPCM
mode. Other entries include dial tone (`0x12`), bong (`0x13`), feedback
(`0x31`) and the 16 DTMF pairs (`0x18` to `0x27`). The current core lookup
rejects non-ADPCM modes, so the attention sample needs a genuine board-ROM
waveform-element decoder and native BUSY/INT0 completion handling. An
independent waveform reconstruction estimates the three attention-tone
elements at approximately 914, 1367 and 1818 Hz over 550 ms in total.
Control/gain and silence-element interpretation still require validation.
No synthetic denial tone has been added, and the reconstructed audio is
not a native or physical playback qualification.

A native isolated off-hook run also observed the terminal request bank 0,
sample `0x14`. The trace reported an unmapped table and no BUSY assertion;
the display subsequently showed the telephone restarting. This confirms the
unsupported sample in the executing device path, but does not by itself
establish the cause of the restart or qualify the subsequent speech prompt.
