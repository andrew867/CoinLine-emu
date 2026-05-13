# Audio boot regression — test plan

## Purpose

Ensure audio device work **does not regress** core boot milestones M0–M10 while adding M6A–M11D. Runs in CI tier **slow** per [`docs/audio-ci-plan.md`](../docs/audio-ci-plan.md).

## Harness contract (normative)

Regression compares traces produced by **`coinline-mame.exe`** runs — not browser captures or standalone JSON generators.

## Prerequisites

- Pinned firmware hash
- Baseline `boot-trace.jsonl` from last known good (stored outside repo or in `fixtures/baselines/` when introduced)

## Test taxonomy

| Class | Description |
| ----- | ----------- |
| **A** | Compare milestone list — must include at least prior set |
| **C** | Same firmware binary; extended runtime for audio milestones |

## Scenario

- `fixtures/scenarios/audio-boot-init.json`

## Commands

```text
tools/windows/build-mame-coinline.ps1
tools/windows/run-coinline-emulator.ps1 -FirmwareBinary ../firmware/flash.bin -RunSeconds 180
tools/windows/validate-boot-milestones.ps1   # when extended for M6A–M11D
```

## Expected traces

- `boot-trace.jsonl` reaches M5+ before audio-specific assertions.
- `audio-trace.jsonl` optional until devices land.

## Pass criteria

- No fewer boot milestones than baseline.
- New milestones additive only.

## Fail criteria

- Boot hang earlier than baseline without documented compatibility item.

## Source files touched

Any file in audio tranches + regression validator scripts.

## Artifact outputs

CI-uploaded run directory; comparison report JSON.
