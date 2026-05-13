# Phrase / command byte 0xB3 (port 0x0061) — source correlation

## Summary

- **0xB3 on the voiceware phrase port** is emulated as the **uPD7759 `port_w` sample index** (MAME `upd7759` standalone/master path). That is a **wire-level** model, not proof of a specific spoken phrase.
- **`0xB3`** is also used as a **VFD glyph** code (umlaut “o”) in the display subsystem — distinct from the voiceware audio phrase index namespace.
- In the same header, **`#define FOLLOW_INSTRUCTIONS_HELP_MSG 179`** — and **0xB3 = 179 decimal** — appears as a **visual / help message table index** (VISTBL* comment rows reference I/F 179). That is **not** automatic proof that the uPD sample index 0xB3 is that help string; it only shows the byte value 179 is overloaded across unrelated tables.
- **Phrase text is unknown** until one of: firmware shows a table mapping 0xB3 → prompt, a verified phrase catalog lines up with the active ROM bank, or decoded audio is matched to a known clip.

## Search notes (integration reference tree)

- Greps for `configure_voice_synthesis`, `VOICE_PROMPT`, and D-log constants hit many call sites; none uniquely tie **sample index 0xB3** to English text without deeper cross-reference.
- Treat **PC near 0x5B29** (if observed in traces) as **hypothesis only** until correlated against traces or optional debug metadata.

## Classification for evidence JSON

- **`phrase_text_status`**: `unknown` for raw index-only observations.
- **`candidate_text`**: empty unless catalog/source confirms.
- **Reason**: document overload of numeric 179 / 0xB3 across display vs voice domains.
