# Forgotten-card alarm and enclosure alerter

## Purpose and hardware boundary

The local reminder is a distinct enclosure transducer path. It is not a spoken handset announcement, not a central-office busy tone, and not the modem's DTMF generator. The same enclosure transducer can also receive incoming ringing. The terminal routes it and generates the warning cadence through hardware outputs; the emulator must not independently infer that a card should be charged, accepted or left in the reader.

The native synchronous contact test is qualified on both board families. There are also existing device-level warning-tone and cadence models, but a genuine terminal-driven forgotten-card alarm, audible enclosure output, and restoration of the ringing path have not been qualified by the available live report.

### Ordinary ringing and the transducer

The January 1997 multi-pay maintenance guide A0667039 (issue 00.01, p. 1-24 and Fig. 1-10) specifies an enclosure dynamic transducer of the same general type as the handset receiver, producing a warbling ringer sound. Pages 7-37 to 7-38 identify the replaceable alerter assembly and acoustic membrane. The normal alerter harness reaches the keypad-board connector; on the earlier optional card-alert configuration it instead passes through the alarm daughterboard. The Desk specification GS5U10 pp. 2-9 and 2-12 independently describes a telephony ringer source and the separate card-alert oscillator, switched onto the same transducer. The normal ringer's oscillator frequencies, alternation rate and chip identity remain unestablished.

The [Mitel MT8840 manufacturer data sheet](https://www.futurlec.com/Datasheet/Zarlink/MT8840AE.pdf) describes a 32 kHz carrier-over-voice ASK modem with an integrated two-tone caller. Its ringer function makes it an interesting period comparison, but it is not a Bell 202/CAS receiver. Neither the reviewed terminal hardware references nor the firmware evidence establishes that this IC was installed. A provisional synthesized warble can represent the known audible behavior; label any specific tone set or modulation rate as an approximation until a reference recording or schematic identifies the actual ringer.

### Acoustic reference measurement

An archived terminal demonstration recording contains a clear ringing burst from approximately 68.95 to 70.90 seconds. Spectral analysis of the 48 kHz decoded audio shows two alternating harmonic families, with inferred repetition frequencies near 491 Hz and 617 Hz. The strongest components are higher harmonics, notably approximately 1.47, 1.85, 3.08, 3.43 and 4.42 kHz, rather than two simultaneous clean sine waves. The tone-family alternation repeats at approximately 10.4 Hz, equivalent to about 20.8 state changes per second or 48 ms per state. This is consistent with a warbler influenced by nominal 20 Hz incoming ringing; the compressed acoustic recording alone does not prove the electrical line frequency or circuit topology.

For a provisional ringer profile, alternate the two harmonic-rich tone families while the incoming ring is actually present, leaving the incoming ring envelope independent of local waveform generation. Preserve distinctive ringing patterns. The approximately 1.95-second recorded burst is an observed example, not a fixed local ring cadence. The oscillator frequencies are inferred from multiple consistent spectral harmonics; the recording and acoustic transducer response do not establish original electrical waveform shape or absolute output level.

## Later-board register contract

| Port | Bit/mask | Function | Active interpretation |
| --- | --- | --- | --- |
| `0xC3` | bit 1 / `0x02` | Warning waveform | The terminal toggles this output from a hardware timer interrupt. Preserve level and timestamp of each transition. |
| `0xC3` | bit 2 / `0x04` | Enclosure transducer source selection | Set selects the warning source; clear restores the incoming-ring source. |
| `0xC3` | bit 3 / `0x08` | Cadence gate | Clear allows the warning waveform through; set mutes the warning waveform. |
| `0x40` (write) | bit 4 / `0x10` | External ringing enable | Set permits the incoming-ring output, subject to actual line and routing state. This latch is distinct from reads at the same I/O address. |

The three warning controls are independent. A toggling waveform with the cadence gate muted must be silent. An asserted warning source without waveform transitions is not an instruction to generate a continuous predetermined tone. Normal card control shares the same port: bit 0 is an auxiliary card control line and bit 1 can be a supply latch clock in an appropriate card-power sequence. Preserve unrelated output bits on every change. Do not let alarm toggles on a card which is already powered continually reset its address or async session.

For the earlier board, the alarm relay can instead be selected by clearing the external-ringing enable output and restored by setting it; a legacy hardware oscillator supplies the tone on some revisions. Do not apply the later board's separate source-select bit to every legacy profile. The exact transducer gain, response and oscillator circuit have not been measured.

### Physical reference: legacy Desk alarm

The Desk terminal general specification, GS5U10 issue 00.02 (February 1997), p. 2-12, documents an HC04 inverter oscillator that drives the enclosure transducer differentially. Its specified dominant component is near **3 kHz**, rather than the later timer-generated 700 Hz reminder. The relay is driven from PIO PC5 through a 74HC14 inverter: high restores the ringing source; low selects the alarm oscillator. The specified alarm loudness is at least 70 phons at 0.5 m, perpendicular to the front face. This is a preliminary product specification, not a measured value for every later Desk board. It also describes incoming-call precedence and inactivity-sensitive on-hook reminders for its profile.

The June 1998 multi-pay replacing-parts guide (P0883897, issue 00.01, p. 3-34 Fig. 3-19) depicts the board-integrated alarm harness interfaces: J2 from the keypad/external-interface hookswitch module and J3 from the alerter assembly. Earlier accessory boards are distinct. These connector identities do not establish the electrical pin assignments. Model the three profile attributes independently: relay source selection, warning oscillator/timer, and transducer routing.

## Timing and sound

On the later board, the actual warning waveform is produced from programmable timer channel 1, not from an emulated audio event generated when the card-presence input changes. The ordinary reminder uses a nominal 700 Hz square wave. The firmware sets the counter for two transitions per tone period. The timer input clock is the effective processor clock divided by 20, so the later 12.288 MHz CPU yields a 614.4 kHz nominal timer input. Quantization and the actual counter/reload semantics may slightly change the measured frequency; use observed edge intervals rather than hardcoding 700 Hz in the renderer.

The user-facing cadence is separate from the waveform frequency. A qualifying reminder has four timing phases under terminal control:

1. A configured delay after going on-hook, with the appropriate separate delay if the card was already present before the call.
2. Warning source selected and cadence gate open for the configured on interval.
3. Cadence gate closed for the configured off interval, then repeated when appropriate. An off interval of zero does not create endless rapid toggling.
4. Total duration expires, or the terminal cancels it: stop the timer waveform, mute the warning source, restore the ring route and allow normal terminal service according to its configured policy.

Configuration comes through the normal feature table (message type 26), with separate duration, on/off cadence and delay fields. Zero duration disables the normal reminder. The firmware selects eligible cards and whether an empty-card reminder is configured. Do not implement that business decision in the audio device or hardcode a chargeable card. A nonfunded contact fixture can be used for hardware-output tests without manufacturing a recognized payment card; native application-triggered acceptance requires an appropriate legitimate test card and configuration.

A coin-call data-jack payment warning also reuses the enclosure transducer. The observed code profile specifies three bursts, nominal 1 kHz, 3 executive ticks on and 4 ticks off, with a 21-tick overall timer. At 10 ms per executive tick these are nominally 30 ms on, 40 ms off and 210 ms total. Keep this separate from the longer configurable forgotten-card cadence. The test may exercise the pin waveform directly without an actual paid call.

## Interaction with other peripherals

- Speaker routing must follow the firmware's warning-source and ring controls; do not play the warning into the handset or uplink it to the far end.
- If incoming ringing is held during a reminder, the ring detector may still see line energy while the external transducer remains under warning control. Do not turn a ringing event into handset off-hook.
- Card removal and terminal cancellation should stop the warning, restore the source selection and preserve the next card's normal initialization.
- Opening the cabinet has a separate operational alarm and can cause the local warning to stop. Preserve genuine switch/report behavior; the presence of an audible beep is not proof of an acknowledged maintenance alarm.
- Ordinary and graphical display configurations share the applicable board peripheral behavior. Distinguish the actual control-board revision from display width.

## Emulator implementation and evidence

Use actual timer/PIO output edges as the primary source of sound. A high-level cadence helper is useful for component tests, but cannot drive the end-to-end test ahead of the firmware. Model the separate alerter output as an audio sink, combine its source selector and mute gate, and keep waveform timing in the native emulated clock domain. Trace the timestamp, port write, old/new masks, timer reload, warning signal, cadence gate, source selection and audible output separately. Measure the peak frequency over several unmuted cycles and the duration of each cadence interval.

| Test | Expected result |
| --- | --- |
| Timer/register-only tone | Native output transitions produce a finite audible signal near the selected frequency, with the correct ring/handset routing. |
| Gate and selector | Toggle bit 1 with gate muted or ring source selected; no warning output. Open the gate with warning source selected; waveform becomes audible. |
| Legacy profile | Use legacy source selection and oscillator behavior where appropriate, without sharing the later register mapping. |
| Configured reminder | Normal terminal UI schedules the delayed on-hook reminder with a legitimate qualifying fixture. Observe cadence, duration and actual audible output. |
| Removal during delay and while sounding | Cancellation and ring-route restoration; no latch-clock induced phantom reset; subsequent card test still works. |
| Incoming ring and data-jack warning | Independently measure incoming ring vs 1 kHz short warning, and verify the handset remains unaffected. |
| End-of-call recovery | Alarm cancellation, next-call dial prompt and ringing policy return to normal. |

Acceptance requires native terminal port transitions and a corresponding audible output; a UI status flag or a synthesized standalone WAV is not sufficient.
