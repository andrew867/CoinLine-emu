# Test plan: Voiceware, voice ROMs, alerter, DTMF

## Unit tests

`test_voiceware_*`, `test_voiceware_rom_inventory_json`, `test_alerter_*`, `test_alerter_dtmf`.

## Integration (may skip)

`test_voiceware_firmware`, `test_alerter_firmware` — skip 77 until Class C harness per `docs/audio-ci-plan.md`.

## Artifacts

`voiceware-trace.jsonl`, `voiceware-output.wav`, `alerter-trace.jsonl`, `audio-trace.jsonl`.

## Failure criteria

Claiming phrase playback proof without WAV energy / trace evidence.
