# Host integration plan

This document describes how the emulator integrates with **CoinLine Host** (a separately-licensed host platform that speaks the same modem-leg protocol the real terminal speaks) to exercise real terminal-to-host protocol flows. Tranche E12 owns this integration.

## Goal

Run the real terminal firmware in the emulator, route its modem UART traffic to a real running CoinLine Host instance, and exercise:

- DLOG submission.
- Table download / upload.
- Rating quotes (where applicable).
- Firmware download (DLA) — gated on operator opt-in.

## Architecture

```
+-----------------+     TCP / WebSocket / pipe / serial     +-----------------+
| coinline-emu    | <----------------------------------->   | coinline (MIT)  |
| (firmware UART) |     bytes only; no source coupling      | HostPlatform.Api|
+-----------------+                                         +-----------------+
```

The host bridge is the **only** integration point. There is no shared library, no shared header, and no compile-time dependency between the two trees. Any coupling is at the wire level only.

## Setup

### Step 1 — Run CoinLine Host

```bash
# launch your host application here
dotnet ef database update --project src/HostPlatform.Infrastructure --startup-project src/HostPlatform.Api
dotnet run --project src/HostPlatform.Api --launch-profile http
```

CoinLine Host listens on `http://localhost:5006/swagger`.

### Step 2 — Run the host-bridge listener

The CoinLine Host project provides a modem-leg gateway that listens on a TCP port and forwards bytes to / from the appropriate session handler. Configure the gateway endpoint and start it. (For details, see the host project's own documentation.)

### Step 3 — Run the emulator

```bash
./coinline-emu \
    -firmware ../firmware/flash.bin \
    -board fixtures/board/board-profile-2line-vfd.json \
    -hostbridge tcp://127.0.0.1:5210 \
    -scenario fixtures/scenarios/table-download.json \
    -evidence out/table-download/
```

The scenario drives the firmware through a complete table-download flow.

## Validation flows

| Flow | Scenario | Asserts |
| ---- | -------- | ------- |
| Boot + idle | `boot-to-idle.json` | M0–M10 reached. |
| Modem connect | `modem-connect.json` | M11 reached against CoinLine Host. |
| Table download | `table-download.json` | Tables persisted in NVRAM/table storage; M12 reached. |
| DLOG submit | (within `card-call.json` and `coin-call.json`) | DLOG entry observed at CoinLine Host endpoint. |
| Card call | `card-call.json` | Card swipe → host call → DLOG submit. |
| Coin call | `coin-call.json` | Coin insertion → host call → DLOG submit. |
| Service mode | `service-mode.json` | Service-mode entry; firmware-defined service-mode operations. |

## Evidence

Each integration test run produces an evidence bundle ([`evidence-bundles.md`](evidence-bundles.md)) containing:

- Boot and I/O traces.
- UART transcript (TX and RX bytes).
- VFD buffer snapshots.
- NVRAM diff (writes since reset).
- Host-bridge transcript.
- CoinLine Host endpoint and request log (if available).
- Scenario result JSON.

## Failure modes

| Failure | Likely cause | Action |
| ------- | ------------ | ------ |
| `Connection refused` on `tcp://127.0.0.1:5210` | CoinLine Host gateway not running | Start CoinLine Host and the modem gateway. |
| Firmware never reaches M11 | Modem state machine bug in emulator | Review the modem JSONL traces under the run directory and confirm DCD/CTS/RTS transitions match expected ASCI semantics. |
| Table download truncated | Carrier loss injection enabled | Disable injection or reproduce the failure mode intentionally. |
| Bytes diverge between sides | Framing mismatch on either side | Compare host-bridge transcript on both sides. |
| Unexpected NCC framing | CoinLine Host protocol version mismatch | Ensure both sides understand the same NCC version per the host project documentation. |

## CI configuration

The nightly job `nightly-host-integration` per [`ci-and-release.md`](ci-and-release.md) brings up CoinLine Host in a container alongside the emulator, runs the validation flows above, and archives evidence bundles.

## License posture reminder

- The host bridge crosses the MIT/GPL boundary. It does not link the two trees. It carries bytes only.
- Anything that would require a shared library between sides is split into independent implementations under each license.

## Cross-references

- [`modem-uart-host-bridge.md`](modem-uart-host-bridge.md), [`../specs/host-bridge.spec.md`](../specs/host-bridge.spec.md).
- [`scenario-runner.md`](scenario-runner.md), [`../specs/scenario-runner.spec.md`](../specs/scenario-runner.spec.md).
- [`evidence-bundles.md`](evidence-bundles.md), [`../specs/evidence-bundle.spec.md`](../specs/evidence-bundle.spec.md).
- [`../test-plans/host-integration-tests.md`](../test-plans/host-integration-tests.md).
