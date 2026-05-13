# Boot-Critical Final Status

Latest analyzed run: `build/runs/20260505T140947-boot-critical`.

Build result: pass. Latest executable SHA256: `a1254ca5e8ba1fbf3faf2c104eccfc648509d808ffd98127645f6d62881287a1`.

Capture: 30 seconds, windowed, audio enabled, trace profile `uart` to avoid slow fetch/stack/vector tracing.

Milestones: M6 reached from real firmware VFD port `0x0060` activity, and M7 reached from firmware-visible external UART response `0x72` (`POWER_ON_ACK`) at port `0x00E1`. M10 is not reached.

Current focused result: `M7_TELEPHONY_BOARD_RESPONDS` is now trace-backed by telephony protocol values `0x72` (`POWER_ON_ACK`), `0xC0` (`TELEPHONY_STATUS`), and `0xC2` (`TELEPHONY_VERSION_NUMBER`), with full variable-length frame responses for `0xC0`/`0xC2`.
