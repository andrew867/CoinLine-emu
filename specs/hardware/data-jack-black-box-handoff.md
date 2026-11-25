# External analogue data jack

## Scope

The data jack is an analogue laptop-modem interface controlled by the terminal. It is not a second management-modem UART and not simply an always-connected telephone jack. The terminal monitors an external modem's connection state, receives its dialled DTMF, routes several analogue relays, presents call setup prompts, and controls the resulting line connection. Both ordinary multi-pay and desktop product profiles have data-jack configurations; the presence of the connector and the actual relay board must be selected independently of the display profile.

An external modem, its DC loop state, the handset, the central-office line and the terminal's internal modem are separate actors. For an initial emulated implementation, a virtual modem that generates genuine line DTMF and consumes analogue PCM suffices. A physical desktop terminal and an isolated analogue line simulator provide the natural reference for later hardware checks. There is no requirement to send calls to real external destinations.

### Physical service references

The Desk general specification GS5U10, issue 00.02 (February 1997), pp. 2-11 to 2-12, explicitly states that the terminal and attached modem share telephone loop current. It describes a user-selected data-call mode (two successive `#` keys), external DTMF dialing and the four distinct setup/connection/loss prompts. Voice prompts are suppressed while the external modem is dialing or connected. These are expected behaviors for the documented Desk profile, not a claim that all later firmware uses the same entry sequence.

The Desk installation/repair guide P0883900, issue 00.01 (June 1998), pp. 4-18 to 4-20 Figs. 4-12 to 4-14, confirms a separate removable daughterboard on the main board, with its external jack on the handset-side edge of the housing. The pocket troubleshooting guide P0883895, issue 03.01 (June 1998), p. 62 Fig. 17, shows a four-position harness connector at J34 on applicable telephony boards and an incorporated loop-current limiting arrangement. The complete electrical pinout and component schematic are still missing.

The Card-based replacing-parts guide P0883899, issue 00.01 (June 1998), p. 5-11, documents that the earlier through-hole board supported either the data jack or the smart-card alert upgrade. The later multi-application board supports the separate integrated interfaces. Preserve this vintage distinction; do not assume every firmware/profile combination has both physical options.

## Later-board register map

| Port | Mask | Direction | Meaning |
| --- | ---: | --- | --- |
| `0x43` | `0x40` | Write | External laptop relay control (R4, board-specific polarity). |
| `0x43` | `0x80` | Write | Network/sink routing control (R5, board-specific polarity). |
| `0x43` | `0x10` | Write | DTMF attenuation/mute control during external digit detection; set selects the high/attenuated state, clear low. |
| `0x40` | `0x80` | Read | External equipment monitor; the downloaded-code profile treats the set bit as equipment present. Other historical builds use the inverted sense. |
| `0x40` | `0x01`, `0x02`, `0x04` | Write | Additional modem and sink/telephone-network relays R1, R2 and R3. |

Reads and writes to `0x40` are separate functions. Preserve shared PIO outputs and read-port status bits. The data-jack monitor is an electrical equipment/loop indication; a UI action that inserts an RJ11 plug does not necessarily imply that an idle modem has gone off-hook. Model physical connection and the external modem's loop/seizure as separate controls, and determine the monitor threshold and polarity from a board capture.

### Relay output profiles

The observed logical actions use the following combined `0x43` masks, with unrelated bits removed. These are firmware output combinations, not a measured schematic of the relay contacts.

| Desired terminal action | Downloaded-code profile, including current universal image | Historical desktop non-download profile | Historical universal non-download profile |
| --- | ---: | ---: | ---: |
| Laptop route on, network route on | `0xC0` | `0x40` | `0x80` |
| Laptop route on, network route off | `0x40` | `0xC0` | `0x00` |
| Laptop route off, network route on | `0x80` | `0x00` | `0xC0` |

Do not extrapolate a fourth physical route from the table without validation. The archived status interpretation for one combination is ambiguous, and the order in which single bits change also matters. Trace raw outputs, intermediate combinations and actual audio/loop effects before claiming R4/R5 contact truth-table fidelity. Earlier boards can have a single relay and a different PIO assignment; do not apply these masks to every desktop or universal image.

## User-visible flow and timing

1. Idle terminal: external modem not seized, normal telephone path available and no synthetic data-jack call.
2. External equipment presents its active loop/signalling indication. Firmware polls and qualifies the monitor; the later relay board avoids sampling during an in-progress relay sequence. Present actual input state rather than directly setting an internal call flag.
3. The terminal switches the laptop and network routes according to its current profile and policy. It can instruct the user to dial and monitor the external DTMF through the analogue detector path. A modem-generated digit is not a physical keypad event.
4. As the number is collected, the terminal selects the appropriate call path and uses existing number screening, rating and call setup logic. The emulator should faithfully provide the electrical events but never manufacture approval, digits, a destination, or an answer indication.
5. During setup the terminal can temporarily route its own modem and sink path, await relay-sequence completion, send digits, then restore the appropriate telephone/network circuit. The external modem responds to actual tones and line supervision.
6. Once connected, transparent bidirectional analogue audio should be possible between the external modem and the office. The handset's state and mute behavior are separate. The monitor and far-end supervision determine disconnection; withdrawing the external loop must not synthesize an unrelated handset event.
7. Cleanup returns the route to its idle combination, clears transient detector state and permits subsequent standalone voice or management operations.

The firmware has a 100 ms monitor-sampling cadence in its data-jack timer path (ten 10 ms executive ticks), with additional qualification across multiple observations, separate display-start delays and disconnect timers. Some setup/disconnect delays are configuration and call-type dependent. A 15-second display initialization value and 1.2/1.5-second examples occur in the design; these are not universal relay-settling specifications. Observe actual native control edges and completion events for each configuration. Do not introduce arbitrary extra half-second blocks into the audio thread to represent relays.

## Implementation phases

### 1. Monitor and routing

Add an independent virtual RJ11 endpoint and external modem loop/seizure input. Feed the selected profile's monitor bit through the ordinary status read. Register actual relay changes and route audio based on the combined latch state. Keep the handset, internal modem and line connection independently represented. Model break/make order and bound switching latency using measured evidence.

### 2. Real digit and supervision exercise

Generate DTMF on the external analogue input, let the terminal detect and interpret it, and use a reserved local test destination. Require a genuine number from the input signal and actual terminal-generated transitions. Test external modem connect/disconnect while the handset stays unchanged. Exercise each route with separate tone sources so that incorrect convergence or crossfeed is visible.

### 3. Data session

Initially connect two laboratory modem endpoints or a virtual analogue modem and a local dial-up test server. The acceptance gate is a stable connected audio path with a modem handshake and bidirectional data. PPP or application networking is a later layer: do not declare a successful modem session solely because an answer tone was played or a TCP connection was established. The desktop hardware is a useful physical reference for line-current indication, relay sequencing, waveform continuity and hangup.

## Test matrix

| Scenario | Required observation |
| --- | --- |
| Disconnected, plugged but idle, and seized external modem | Independent monitor behavior with correct board polarity; no fictitious handset transition. |
| Stable and short monitor transitions | Native debounce and recovery; short pulses do not inevitably become calls. |
| Relay switching | Trace R1-R5 raw register values, intermediate states, route, completion timing and line supervision. No unintended path during intermediate writes. |
| Laptop-generated digits | DTMF from PCM is detected once, without duplicate keypad input or direct mutation of the digit collector. |
| Local allowed free route | Native call setup and local answer; transparent bidirectional modem tones/data. |
| Invalid destination and external disconnect | Normal native rejection or disconnect and restoration of the correct idle routes. |
| During an ordinary voice call or management session | Correct isolation and ownership of modem, sink, handset and external jack; no cross-delivery of NCC bytes. |
| Repeated cycles, earlier/later/desktop profiles | No stuck relay, stale equipment indication, lost ring capability or next-call failure. |
| Short enclosure payment-warning waveform | Validate the separate alerter output and no handset/line leakage; a paid-call demonstration is a different milestone. |

Trace actual port reads/writes, laptop-loop transitions, handset state, relay sequence, audio source and sink, real dialled tone events, answered/disconnected state and next-call recovery. An application saying a data-jack call is active is not by itself proof of electrical interoperability.
