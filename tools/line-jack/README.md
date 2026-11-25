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

From the emulator to the office:

- `H1` / `H0` — handset off-hook / on-hook
- `E1` / `E0` — firmware ringer enable
- `S` / `X` — loop seized / released
- `Q1` / `Q0` : modem relay connected / disconnected, independent of handset seizure. On a management dial, `Q0` restores the CO to its voice-ready state even when the handset continues holding the loop.
- `D<digit>` — digit the firmware outpulsed
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

### Local ADSI service

The optional `--adsi` launcher switch enables the exact local route in the
`[adsi]` section of `coinline.ini`. It also works in local CO mode without
credentials. The aliases must be permitted by the installed terminal tables.
The office sends an unauthenticated UDP SIP INVITE to the configured loopback
service, offers PCMU at 8 kHz, and bridges actual RTP audio. It reserves the
advertised receive socket before dialing. Post-answer DTMF events from the
terminal, including A through D, are transmitted as audio. A failed local
route never falls through to the external SIP or PSTN destination.

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

Emulator path: dial `2130706433` with `COINLINE_NCC_LAB_HELLO=1` (deferred +0.4 s
frame on the same socket). Do not set `COINLINE_OUTBOUND` to that IPv4 decimal —
the office skips voice answer for management dials so the terminal is not pulled
onto the voice-answered path.
