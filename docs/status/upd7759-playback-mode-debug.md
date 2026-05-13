# uPD7759 Playback Mode Debug

Last updated: 2026-05-05

## Findings

- `VOICE_SYNTHESIS_CODE_ADDR` writes (`0x61`, `0xB3`) are real firmware writes (M5V/M5A).
- Current integration uses standalone-style start strobes (`port_w`, `start_w(0)`, `start_w(1)`).
- Slave-mode forcing was removed earlier because MAME slave path requires DRQ streaming and mismatched this firmware path.
- Voice traces show repeated starts with periodic idle transitions (`upd7759_idle_start_edges`), consistent with repeated phrase retries/cadence.

## In this prompt

- Reconfirmed playback lifecycle artifacts across 3 runs:
  - `port_0x61_0xB3_count`: 3044
  - interval: 363327 cycles (stable)
  - audible clicking corresponds to repeated command lifecycle, not a single sustained segment.

## Conclusion

- uPD7759 path is active but currently coupled to scheduler/interrupt gate.
- No new evidence that mode switch alone resolves M6; primary blocker remains pre-EI scheduler loop.
