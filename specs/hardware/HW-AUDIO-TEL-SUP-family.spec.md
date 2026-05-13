# Umbrella spec: Audio routing, telephony decode, supervision

Hardware IDs: **HW-AUD-001**, **HW-TEL-001**, **HW-SUP-001**

## Audio routing (HW-AUD-001)

- **Sources:** `millennium_audio_route*.cpp`, `millennium_audio_route_apply.*`.  
- **State:** Composite route/mute from `fixtures/board/audio-routing-state-map.json`.  
- **Traces:** `audio-route-trace.jsonl`, mute events in `millennium_audio_trace.*`.  
- **Tests:** `test_audio_route_fixture.cpp`, `test_audio_call_state_fixture.cpp`; firmware tests may skip 77.  
- **Acceptance:** `call_state_if_known` derived from devices — **no** scenario-owned fake call state.

## Telephony (HW-TEL-001)

- **Sources:** `millennium_telephony.cpp`.  
- **Tests:** `test_telephony_decode.cpp`.  
- **Trace:** `telephony-trace.jsonl`.

## Supervision (HW-SUP-001)

- **Sources:** `millennium_supervision*.cpp`, `millennium_supervision_fsm.*`.  
- **Map:** `fixtures/board/disconnect-supervision-map.json`.  
- **Tests:** `test_supervision_fixture.cpp`, integration firmware test (skip rules apply).  
- **Acceptance:** `disconnect_event` only from honest status/carrier fusion — no injected teardown without bytes.
