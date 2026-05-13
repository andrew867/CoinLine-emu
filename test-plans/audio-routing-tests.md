# Test plan — Audio routing / telephony commands

## Proof strategy

| Proof tier | Requirement |
| ---------- | ----------- |
| **A — ROM execution** | Running ROM issues telephony encode calls; capture **byte sequence** on the logical bridge with cycle timestamps. |
| **B — Trace replay** | Replay captured host→processor byte stream from hardware validation; gate snapshot must match golden file. |

Each **AR-xx** case requires **A or B**.

## Preconditions

- Telephony bridge instantiates modeled gates (`gate_tx`, `gate_rx`, `sidetone_mode`, `dtmf_from_pad`, `rx_gain_step`).
- Initial shadow documented per profile.

## Cases

| ID | Name | Steps | Pass criteria |
| -- | ---- | ----- | ------------- |
| AR-01 | Idle baseline | Tier A: power stable idle **or** Tier B: idle fixture | Known gate snapshot |
| AR-02 | Off-hook | Inject `OFF_HOOK` (`0x62`) per decode rules | `gate_rx`/`gate_tx` match profile matrix |
| AR-03 | Voice conditioning | **Must use Tier A or replay from ROM capture** | Exact order **`0x29`→`0x2A`→`0x28`** for prompt-on; **`0x29`→`0x2B`→`0x28`** for prompt-off |
| AR-04 | TX mute independence | Issue `0x27` then `0x28` | `gate_tx` closed, `gate_rx` open |
| AR-05 | RX mute independence | Issue `0x29` then `0x26` | `gate_rx` closed, `gate_tx` open |
| AR-06 | CALL_STATE establish | Send `0x44` / `0x45` / `0x46` / unsupervised variants | Billing shadow updates |
| AR-07 | Line→earpiece | Enable CO PCM inject + `gate_rx` open | PCM appears only at earpiece sink |
| AR-08 | Voice→earpiece | Enable phrase marker + conditioning sequence | Phrase energy only after conditioning completes |
| AR-09 | DTMF gate | `0x2E` then `0x2D` | DTMF generator toggles independently of voice |
| AR-10 | Volume staging | `0x20`–`0x25` stream | Monotonic `rx_gain_step` per spec |

## Fixtures

- `fixtures/board/audio-routing-state-map.json`
- Golden telephony byte trace (**`compatibility_validation_required`** until checked in)

## Failure modes

- Opcode ordering deviation without logged **`routing_error`** → fail.
