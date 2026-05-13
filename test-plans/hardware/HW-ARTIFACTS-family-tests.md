# Test plan: Evidence bundles, artwork assets

## Tests

`test_evidence_bundle` (integration), `test_evidence_bundle_export_tool`, `test_millennium_artwork_assets`.

## Evidence bundle export CLI

`evidence-bundle-export` with `--from-run-dir` vs TCP payload — see `tools/evidence-bundle-export/README.md`.

## Failure criteria

Bundle validator passes without mandatory audio files when `expect_audio_traces: true`.
