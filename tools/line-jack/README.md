# Line jack central office

The [Windows launcher](../../RUNNING.md) starts this process with the bundled Python runtime. Local CO mode is the default. Add the SIP settings to your local `coinline.ini` and pass `--sip` for an external call. The launcher supplies an independent loopback jack for each running profile.

The emulator opens a TCP jack on `127.0.0.1` when `COINLINE_LINE_PORT` is set
(for example `4010`). A host program connects as the central office.

## Protocol

From the office to the emulator:

- `R1` / `R0` — ring detector asserted / released
- `C1` / `C0` — line connected / open
- `V0` / `V1` / `V` — polarity-reversal phase 0, phase 1, or clear
- `W<hex>` — 8 kHz signed little-endian PCM toward the handset
- `T1` / `T0` mark when that PCM is CO dial tone or ordinary line audio.
- `B1` / `B0` : enable or disable the bounded direct-voice playout cushion.

From the emulator to the office:

- `H1` / `H0` — handset off-hook / on-hook
- `E1` / `E0` — firmware ringer enable
- `S` / `X` — loop seized / released
- `Q1` / `Q0` : modem relay connected / disconnected, independent of handset seizure. On a management dial, `Q0` restores the CO to its voice-ready state even when the handset continues holding the loop.
- `D<digit>` : genuine controller or modem line-tone event, used as initial outpulse before answer and connected DTMF after answer
- `M<hex>` — 8 kHz PCM from the handset microphone

## Local office

```bash
export COINLINE_LINE_PORT=4010
export COINLINE_OUTBOUND=5551234
export COINLINE_CO_MODE=local
python tools/line-jack/central_office.py
```

Optional inbound ring after `E1`:

```bash
export COINLINE_CO_INBOUND=1
```

Inbound mode holds `R1` until the handset answers. A wall-clock ring
cadence is too short for a slow emulator host and never reaches the
firmware `Ringing` glass.

## SIP bridge

Credentials stay in the environment. Never commit them.

### Local SipModem (lab)

Invite the CoinLine SipModem on the host (Docker maps UDP 5060) without a
registrar. The payphone still dials a short local number; the office places
the INVITE and bridges G.711 RTP ↔ jack PCM. Decoded Bell 202 bytes land on
the NCC listener.

```bash
export COINLINE_LINE_PORT=4010
export COINLINE_OUTBOUND=5551234
export COINLINE_SIP_DEST=local
export COINLINE_SIP_PEER=127.0.0.1:5060
export COINLINE_CO_MODE=sip
python tools/line-jack/central_office.py
```

### Registrar / softphone

```bash
export COINLINE_SIP_HOST=...
export COINLINE_SIP_USER=...
export COINLINE_SIP_PASSWORD=...
export COINLINE_CO_MODE=sip
export COINLINE_SIP_MY_IP=10.0.0.2          # Contact/SDP
export COINLINE_SIP_LOCAL_PORT=5070        # avoid host 5060 (SipModem)
# Needs a Python with pyVoIP (MSYS python often lacks it).
python tools/line-jack/central_office.py
```

The registrar bridge calls the destination outpulsed on the jack. It converts
10- and 11-digit North American dials and 011 international dials to E.164.
For a fixed-target lab probe, set `COINLINE_SIP_DEST` and set
`COINLINE_OUTBOUND` to the exact trigger digits.

Connected DTMF uses `COINLINE_SIP_DTMF_MODE=auto` by default. An accepted
8 kHz `telephone-event` SDP mapping enables RFC 4733 on the same RTP
stream, using a constant event onset timestamp and three final packets.
If the peer permits INFO and explicitly advertises `application/dtmf-relay`,
the bridge can use the existing dialog route for legacy SIP INFO instead.
The fallback is an in-band G.711 waveform. Override with `inband` or `info`
only for a known peer. An INFO method listing alone does not negotiate a
legacy DTMF body. Each connected digit is sent using exactly one transport.

### Local ADSI service

The optional `--adsi` launcher switch enables the exact local routes in the
`[adsi]` and `[adsi.routes]` sections of the profile INI. A shared
`adsi-routes.ini` can replace just these sections for both profiles, without
altering their SIP accounts or saved terminal data. Copy and edit
`adsi-routes.example.ini` for the dedicated service. Each key in `[adsi.routes]`
is the SIP extension, and each comma-separated value is an exact dialed alias.
The legacy single-target `dial_digits`/`target` settings still work. Aliases
must be allowed by installed terminal tables (prefer exact no-payment rules).
The office sends an unauthenticated UDP SIP INVITE to the configured loopback
or private IPv4 service, offers PCMU at 8 kHz, and bridges actual RTP audio.
It binds signaling and the reserved RTP socket to the interface used to reach
that service. SIP responses, Contact and SDP media must agree with the
configured host; arbitrary public destinations and route sets are rejected.
Post-answer DTMF events from the
terminal, including A through D, are transmitted as 60 ms tones with the
ADSI interdigit cadence. The direct path delivers full received RTP audio
frames and uses a small native playout cushion instead of independently
zero-padding an empty audio queue. A failed local
route never falls through to the external SIP or PSTN destination. The numeric
NCC modem destination remains separate and cannot be a voice alias.

The graphical receiver examines the received PCM for the CAS tones and Bell
202 serial data; neither the CO nor the launcher provides display text or
fabricated terminal acknowledgements. A live graphical firmware exchange is
required to qualify the complete service.

Inbound SIP INVITEs set ring on the jack until the handset answers; then the
office answers SIP and applies polarity reversal. For a loopback ring probe
against a listening UA: `python tools/line-jack/sip_invite_ring.py`.

`COINLINE_CO_MODE=auto` (default) uses SIP when host/user/password are set, or
when `COINLINE_SIP_DEST=local`.

## NCC lab frame

```bash
python tools/line-jack/ncc_lab_frame.py
```

Sends one terminal-matched hello to `COINLINE_NCC_PORT` (default 4000) so the
API shows `lastFrameSampleHex` and `terminalMatched`. Use after a management
dial has already proven TCP connect, or alone as a listener smoke.

The old `COINLINE_NCC_LAB_HELLO` emulator switch has been retired. The helper
above is a separate explicit lab tool; a running terminal sends only its own
serial bytes. Do not expect a synthetic hello on its management socket.
