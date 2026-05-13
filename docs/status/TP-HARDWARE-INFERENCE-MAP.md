# TP Hardware Inference Map

## Confidence Legend

- `High`: strongly supported by part role + placement + observed traces.
- `Medium`: plausible with supporting evidence but not fully proven.
- `Low`: candidate only; requires contradiction testing before implementation lock.

## TP-Side Inference

| Item                                                                     | Confidence | Evidence                                        | Validation                                 |
| ------------------------------------------------------------------------ | ---------- | ----------------------------------------------- | ------------------------------------------ |
| `U13` PCD3349A is TP control origin                                      | High       | chip role + board location near TP interconnect | CP query/response and key byte latency     |
| `U5` (`74HC151`) participates in scan source selection                   | High       | digital mux adjacent to TP and keypad path      | scan-step perturbation vs TP byte output   |
| `U16` (`74HC4051`) affects telephony audio path selection                | Medium     | analog mux placement near TP/audio region       | hook/alert/tone scenario deltas            |
| Rev2 forgotten-card may traverse in-line alerter->hookswitch->keypad->TP | Medium     | Rev2 field observation + board topology         | card-left/on-hook test with TP/CP evidence |

## PCP-Side Inference

| Item                                               | Confidence | Evidence                               | Validation                               |
| -------------------------------------------------- | ---------- | -------------------------------------- | ---------------------------------------- |
| Z180 + glue cluster is CP bus/timing center        | High       | U6 + adjacent transceivers/latches/PLD | poll cadence and timeout counters        |
| SRAM (`U4`) backs NCC downloaded data              | Medium     | mode behavior reports + SRAM role      | NCC vs demo path trace divergence        |
| EEPROM (`U17`) stores persistent config gate state | Medium     | device capability                      | power-cycle persistence checks           |
| Exact PLD equations/gate directionality            | Low        | no netlist/equations                   | infer by contradiction via timing traces |

### Companion ICs (telephony PCP — lab inference)

These entries record **placement-led hypotheses** for Rev-class telephony boards. They are **not** substitute for schematic/BOM confirmation.

| Item | Confidence | Evidence | Validation |
| ---- | ---------- | -------- | ---------- |
| **`U9`** — coin relay / high-side drive helper (often a small 8-pin MCU-class part near the coin relay driver) | Medium | proximity to relay/coin power path; industry norm for boosted coil drive | relay timing vs coin validator traces; absence of stuck relay in soak |
| **`U10`** — analog mux / audio routing near SLIC/telephony analog cluster (sometimes OEM-marked) | Medium | placement beside analog switches / voice paths | hook/tone/voice route A/B with audio captures |

### Industry parallels (non-attributed)

Payphone and desktop terminal products of this era commonly combined: (a) **dual-tone coin signaling** generated under Bell-core-style level constraints, often from a small OTP/MTP microcontroller; (b) **relay coil boosting** or constrained drivers implemented with compact helper MCUs; (c) optional **1200-class modem** channels (for example Bell 103-compatible host links) **separate from** on-hook DTMF generation in the TP. Those patterns inform confidence labels above but do **not** prove a specific supplier mask ROM without hardware evidence.

See also [TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md](TP-PCD3349A-BEHAVIORAL-ROM-STATUS.md) for emulator-side TP behavior.

## Connector Notes

- Rev2 target assumptions:
  - keypad and handset paths terminate on telephony PCP then serialize to CP.
- Rev1 manual references:
  - retained as non-authoritative compatibility notes only.

## Divergence Policy

- If Rev1 and Rev2 hypotheses conflict, Rev2 board + firmware runtime evidence wins.