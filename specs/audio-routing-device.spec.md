# Spec — Audio routing (telephony processor command bridge)

## Purpose

Model handset/line audio routing as **command effects** + **supervision inputs**.

## Route state model

| Route | States |
| ----- | ------ |
| TX path | `muted` / `unmuted` |
| RX path | `muted` / `unmuted` |
| Sidetone processor mode | `suppression_on` / `suppression_off` |
| Keypad DTMF | `enabled` / `disabled` |
| Call classification | `idle`, `dialing`, `established_supervised`, `established_unsupervised`, … |

## Mute state model

Independent booleans for TX and RX gates; sidetone flag orthogonal.

## Line / handset / voice path model

```
CO audio <---> telephony front-end <---> modeled subscriber ports
Voice playback ---> summed into RX path subject to RX mute/sidetone sequence
```

Analog hybrids/SLIC specifics out of scope—behavioral parity only.

## Input / output signals

### Inputs

- Host→processor command bytes (`fixtures/board/audio-routing-state-map.json`).
- Hookswitch / continuity updates from processor→host stream.

### Outputs

- Effective gates applied to PCM sinks (handset earpiece / line tap evidence).

## Port / bit mappings

Logical mapping only—physical UART/SIO framing is **`compatibility_validation_required`** until traced.

## Transition tests

See [`../test-plans/audio-routing-tests.md`](../test-plans/audio-routing-tests.md).

## Call-state tests

Couple `CALL_STATE_*` transitions with billing scenario fixtures.

## Acceptance criteria

1. Every command byte mutates modeled gates deterministically.
2. Voice-prompt conditioning sequence (`RX_MUTE`→`SIDETONE_*`→`RX_UNMUTE`) replay matches trace order.
