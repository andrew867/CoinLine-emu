# Compatibility validation items

## Status semantics — read this first

Each row carries **two independent status columns**, plus an optional certification status:

| Column | Meaning |
| ------ | ------- |
| **Emulator implementation status** | Whether `coinline-emu` implements the behavior: `complete`, `in_progress`, `pending`, `regressed`. |
| **Field validation status** | Whether the behavior has been observed on physical hardware: `complete`, `in_progress`, `pending`. |

> **Hardware validation pending never means code is missing.** A row may have `Emulator implementation status = complete` and `Field validation status = pending` simultaneously. Code merges are gated on the emulator status only. Field validation is an independent workflow performed by certification engineers and recorded here for traceability.

## Column glossary

- **ID** — `CV-` prefix and a four-digit sequence.
- **Area** — subsystem under [`docs/`](.) (e.g., VFD, Keypad, Modem, NVRAM, Tables, Host).
- **Question** — the specific compatibility question being validated.
- **Why it matters** — the customer-visible behavior or risk.
- **Evidence needed** — the artifacts required to mark the row complete.
- **Emulator implementation status** — see above.
- **Field validation status** — see above.

---

## Boot

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0001 | Boot | Does the supported terminal firmware load and execute its reset vector? | Smoke test for any device compatibility claim. | Boot trace shows M0 + M1; SHA-256 logged. | pending | pending |
| CV-0002 | Boot | Does startup code initialize RAM and Z180 registers without aborting? | Required for any further behavior. | Trace shows M2–M4. | pending | pending |
| CV-0003 | Boot | Does the firmware reach the idle display? | Confirms the supported payphone model is fully bootable. | M10 reached; VFD buffer matches `fixtures/display/vfd-2line-idle.json` or `vfd-11line-ad.json`. | pending | pending |

## Display (VFD)

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0010 | VFD | Does the 2-line VFD render the expected idle text? | Field-visible compatibility. | Buffer snapshot matches reference. | pending | pending |
| CV-0011 | VFD | Does the 11-line VFD render advertising frames correctly? | Field-visible compatibility. | Buffer snapshot matches reference. | pending | pending |

## Keypad and front-panel inputs

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0020 | Keypad | Are all numeric keys recognized? | User input compatibility. | Per-key fixture round-trip. | pending | pending |
| CV-0021 | Keypad | Are quick-access keys recognized? | User input compatibility. | Per-key fixture round-trip. | pending | pending |
| CV-0022 | Hookswitch | Does on-hook / off-hook propagate? | Call lifecycle. | Lift / hangup scenario. | pending | pending |
| CV-0023 | Security | Are lock / door / vault / service inputs read? | Service workflow compatibility. | Each input toggled and observed. | pending | pending |

## Modem and host bridge

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0030 | Modem | Does the firmware program the UART correctly? | Required for any host call. | M8 reached. | pending | pending |
| CV-0031 | Modem | Does the firmware traverse modem states (idle → dialing → connected → idle)? | Call lifecycle. | Modem state log. | pending | pending |
| CV-0032 | Host bridge | Does the host bridge transparently move bytes between firmware and CoinLine Host? | Required for any protocol-level interoperability. | Loopback round-trip. | pending | pending |
| CV-0033 | Host bridge | Are the modem control signals (DCD/CTS/RTS/DTR) honored? | Protocol-level interoperability. | Signal log per scenario. | pending | pending |

## Card and smart-card

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0040 | Card | Does a valid magstripe swipe complete? | Card-call compatibility. | `magcard-valid.json` scenario passes. | pending | pending |
| CV-0041 | Card | Is an invalid LRC rejected? | Failure mode compatibility. | `magcard-invalid-lrc.json` scenario triggers expected firmware response. | pending | pending |
| CV-0042 | Smart card | Is the ATR honored? | Smart-card compatibility. | `smartcard-valid.json` scenario completes. | pending | pending |

## Coin and alerter

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0050 | Coin | Are all denominations recognized? | Coin-call compatibility. | Per-denomination fixture round-trip. | pending | pending |
| CV-0051 | Coin | Are coin error states handled? | Failure mode compatibility. | Error fixture. | pending | pending |
| CV-0052 | Alerter | Does the alerter audio fire on expected events? | Field-audible compatibility. | Audio sample exported in evidence bundle. | pending | pending |

## NVRAM and tables

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0060 | NVRAM | Does NVRAM survive reset? | Configuration durability. | Persistence test. | pending | pending |
| CV-0061 | NVRAM | Does the firmware recover from a corrupted NVRAM checksum? | Field robustness. | `corrupt-checksum.nvram.json` scenario. | pending | pending |
| CV-0062 | Tables | Do downloaded tables persist? | Table-distribution compatibility. | `table-download.json` scenario. | pending | pending |

## Host integration

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0070 | Host | Does a host call complete with CoinLine Host? | End-to-end interoperability. | Modem connect scenario against running CoinLine Host. | pending | pending |
| CV-0071 | Host | Does table download persist after disconnect? | End-to-end durability. | Table download scenario, reset, observe NVRAM diff. | pending | pending |
| CV-0072 | Host | Does DLOG submission appear at CoinLine Host? | End-to-end logging. | DLOG submit scenario, observe entry at CoinLine Host. | pending | pending |

## Frontend and artwork

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0080 | Artwork | Are clickable regions aligned with the front-panel image? | Operator UX consistency. | Screenshot evidence. | pending | pending |

## Audio / voice / supervision

Rows correspond to **explicit OPEN / compatibility_validation_required** markers in [`fixtures/board/audio-device-map.json`](../fixtures/board/audio-device-map.json), routing maps, supervision maps, and [`specs/audio-trace-format.spec.md`](../specs/audio-trace-format.spec.md). Implementation follows [`audio-implementation-roadmap.md`](audio-implementation-roadmap.md).

| ID | Area | Question | Why it matters | Evidence needed | Emulator implementation status | Field validation status |
| -- | ---- | -------- | -------------- | --------------- | ------------------------------ | ---------------------- |
| CV-0091 | Voiceware | Does phrase timing and completion signaling match the supported profile for every triggered prompt? | Prompt cut-off or stuck-busy breaks call flows. | `voiceware-trace.jsonl` start/complete pairs; optional timing table in profile. | pending | pending |
| CV-0092 | Voiceware | Is the voice-playback completion interrupt path verified end-to-end for the supported profile? | Missed completion stalls higher-level state machines. | Trace shows IRQ or polled status per interrupt map; milestone M11B. | pending | pending |
| CV-0093 | Audio route | Does the default on-reset routing matrix match the supported profile? | Wrong idle routing masks line vs handset faults. | Trace `route_state` vs `fixtures/audio/audio-route-idle.json`; M10A. | pending | pending |
| CV-0094 | Audio route | Are microphone and earpiece mute behaviors independent as defined by the profile? | Incorrect muting leaks audio or blocks legitimate paths. | `mute_change` events vs scenario replay; M11C. | pending | pending |
| CV-0095 | Telephony bridge | Is the telephony processor bridge (`telephony_processor_bridge` in audio-device-map) fully wired for handset/line audio when the profile requires it? | Routing gaps break voice paths even when voiceware works. | Cross-trace modem + route + host-bridge where applicable. | pending | pending |
| CV-0096 | Supervision | Can normal disconnect vs CPC-class events be distinguished when the profile defines both? | Wrong teardown classification affects redial and fault handling. | `supervision-trace.jsonl` typed events; fixture trio `disconnect-*.json`; M11D. | pending | pending |
| CV-0097 | Supervision | Are loop/current/polarity/CPC timing thresholds validated against measured bounds for the supported deployment? | Emulator thresholds must not be guessed. | Lab timing capture or explicit profile caps recorded in compatibility ledger. | pending | pending |
| CV-0098 | Alerter | Are service vs error cadences emitted only through documented I/O-side effects? | Fake tones bypass conformance proof. | `alerter-trace.jsonl` matches `fixtures/audio/alerter-*.json`. | pending | pending |

---

## How to update this ledger

- Each row's status changes only with a referenced PR or evidence bundle.
- A row moves to `complete` when the referenced test passes in CI **and** the corresponding evidence artifact is attached. Field validation status moves independently when certification engineers attest.
- Adding a row requires a corresponding test or evidence-artifact reference.

## How to use this ledger

- **Engineers** scan `Emulator implementation status` to find the next item to bring to `complete`.
- **Validation engineers** scan `Field validation status` for items ready for hardware confirmation.
- **Operators** read the table to understand precisely what is and is not validated.

## Cross-references

- [`risk-register.md`](risk-register.md) — risks that map to specific items.
- [`acceptance-test-plan.md`](acceptance-test-plan.md) — gate-level rollup of these items.
- [`boot-milestones.md`](boot-milestones.md) — milestone-level rollup.
