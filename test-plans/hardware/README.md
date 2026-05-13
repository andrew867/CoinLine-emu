# Hardware test plans (`test-plans/hardware`)

Umbrella test plans mirror [`specs/hardware/README.md`](../../specs/hardware/README.md). Each file lists unit tests, integration tests, MAME runs, fixtures, failure criteria, artifacts, and commands.

| Test plan file | Spec umbrella |
| ---------------- | ------------- |
| [HW-Z180-family-tests.md](HW-Z180-family-tests.md) | HW-Z180-family.spec.md |
| [HW-MEMORY-family-tests.md](HW-MEMORY-family-tests.md) | HW-MEMORY-family.spec.md |
| [HW-IO-ASCI-MODEM-family-tests.md](HW-IO-ASCI-MODEM-family-tests.md) | HW-IO-ASCI-MODEM-family.spec.md |
| [HW-VFD-family-tests.md](HW-VFD-family-tests.md) | HW-VFD-family.spec.md |
| [HW-INPUT-PANEL-family-tests.md](HW-INPUT-PANEL-family-tests.md) | HW-INPUT-PANEL-family.spec.md |
| [HW-PAYMENT-family-tests.md](HW-PAYMENT-family-tests.md) | HW-PAYMENT-family.spec.md |
| [HW-VOICE-ALERT-family-tests.md](HW-VOICE-ALERT-family-tests.md) | HW-VOICE-ALERT-family.spec.md |
| [HW-AUDIO-TEL-SUP-family-tests.md](HW-AUDIO-TEL-SUP-family-tests.md) | HW-AUDIO-TEL-SUP-family.spec.md |
| [HW-ARTIFACTS-family-tests.md](HW-ARTIFACTS-family-tests.md) | HW-ARTIFACTS-family.spec.md |

**Global commands:** `cmake --build <builddir>`, `ctest --output-on-failure`; MAME builds via `tools/windows/build-mame-coinline.ps1`.
