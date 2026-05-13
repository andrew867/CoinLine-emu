# Umbrella spec: Evidence bundles, screenshots, artwork

Hardware IDs: **HW-EV-001**, **HW-SCN-001**, **HW-ART-001**

## Evidence export (HW-EV-001)

- **Sources:** `millennium_evidence_bundle.*`, `tools/evidence-bundle-export/*`.  
- **Spec:** `specs/evidence-bundle.spec.md`.  
- **Tests:** `tests/integration/test_evidence_bundle.cpp`, `test_evidence_bundle_export_tool`.  
- **Acceptance:** Bundles reflect **coinline-mame** runs (TCP framed JSON or `--from-run-dir`), not browser mocks.

## Screenshot harness (HW-SCN-001)

- **Script:** `tools/windows/run-screenshot-capture.ps1`.  
- **Acceptance:** PNG paths recorded in run report; GUI capture environment-dependent.

## Artwork / layout (HW-ART-001)

- **Files:** `src/mame/layout/millennium.lay`, `artwork/README.md`.  
- **Tests:** `test_millennium_artwork_assets.cpp`.  
- **Acceptance:** Click targets documented in `docs/artwork-and-layout.md` — visual parity not claimed as complete.
