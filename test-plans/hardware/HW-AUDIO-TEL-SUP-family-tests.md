# Test plan: Audio routing, telephony, supervision

## Unit tests

`test_audio_route_fixture`, `test_audio_call_state_fixture`, `test_telephony_decode`, `test_supervision_fixture`.

## Integration (may skip)

`test_audio_route_firmware`, `test_supervision_firmware`.

## Artifacts

`audio-route-trace.jsonl`, `telephony-trace.jsonl`, `supervision-trace.jsonl`, `mute_change` / `route_change` events in `audio-trace`.

## Failure criteria

Injected `disconnect_event` without processor/host byte path.
