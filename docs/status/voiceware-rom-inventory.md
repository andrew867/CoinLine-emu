# Voiceware ROM inventory

- **Authoritative file**: `build/generated/voiceware-rom-inventory.json` (written by `tools/windows/run-screenshot-capture.ps1` from the `U16`/`U26` inputs you pass in).
- **MAME `ROM_START`**: `voice_a.bin` at `0x000000` (1 MiB) + `voice_b.bin` at `0x100000` (1 MiB) in region `voicew` (2 MiB) — `src/mame/coinline/millennium.cpp`.
- **Phrase catalogs** under `fixtures/voiceware/*.json` are **stubs** until a backed decode links samples to text.

