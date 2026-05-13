# Front Panel Input Status

Last updated from run: `build/runs/20260505T140947-boot-critical`.

## Current State (Evidence-Backed)

- Hookswitch: not proven firmware-visible (no input transitions captured in latest run)
- Keypad: not proven firmware-visible (no digit/star/pound events captured)
- Service/In-service: not implemented/proven in capture

## Evidence

- `front-panel-trace.jsonl` exists but is empty in latest run:
  - `build/runs/20260505T140947-boot-critical/front-panel-trace.jsonl`
- Input mapping snapshot:
  - `build/runs/20260505T140947-boot-critical/input-resolution.json`

## Exact Missing Pieces

- A firmware-visible input port/bit model for:
  - hook state (on-hook/off-hook)
  - keypad matrix or key latch (0-9, star, pound)
  - optional service switch (only if fixture/trace-backed)
- A capture script that generates deterministic input transitions during the 30s run and logs them to `front-panel-trace.jsonl`.

