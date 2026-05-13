# Test plan: VFD family (HW-VFD-001, HW-VFD-002)

## Reference parity extension (HW-VFD-003, HW-VFD-004)

| Spec | Test plan |
| ---- | --------- |
| [`../../specs/hardware/HW-VFD-reference program revision-PARITY.spec.md`](../../specs/hardware/HW-VFD-reference program revision-PARITY.spec.md) | [`HW-VFD-reference program revision-PARITY-tests.md`](HW-VFD-reference program revision-PARITY-tests.md) |

Use **TP-VFD-003-*** / **TP-VFD-004-*** case IDs for gate evidence. Baseline rows below remain for legacy HW-VFD-001/002 naming.

## Unit tests (Class A)

| Test binary | Role |
| ----------- | ---- |
| `test_vfd_command_decode` | Command/state machine vs fixtures |
| `test_vfd_2line_idle` | Idle text snapshot |
| `test_vfd_buffer_snapshot` | RAM buffer coherence |
| `test_vfd_11line_ad` | 11-line profile parsing |

**Failure criteria:** Fixture mismatch; decode asserts without firmware justification.

## Integration / firmware-facing (Class C)

| Test | Notes |
| ---- | ----- |
| Manual / scripted MAME run | Requires `coinline-mame`, flash.bin, overlay |

**Failure criteria:** Claiming **M6** without `boot-trace.jsonl` milestone line + PNG evidence.

## Real MAME run

```powershell
# After overlay + build — see docs/status/remaining-work-estimate.md
& '.\tools\windows\run-screenshot-capture.ps1' -FirmwareBinary '...\firmware source tree\flash.bin' -RunSeconds 180 -Screenshot ...
& '.\tools\windows\validate-boot-milestones.ps1' -RunDir $out
```

## Fixtures

`fixtures/display/*.json`, `fixtures/board/board-profile-*-vfd.json`

## Artifacts

`boot-trace.jsonl`, `vfd-trace.jsonl`, `boot-normal-start.png`, validator stdout.

