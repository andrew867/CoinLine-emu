# `fixtures/`

This directory holds JSON fixtures, board profiles, modem byte streams, NVRAM images, card payloads, scenario files, and expected display outputs. Fixtures are data — no compiled code lives here.

## Subdirectories

| Subdirectory | Purpose | Tranche |
| ------------ | ------- | ------- |
| `board/` | Memory map, I/O port map, interrupt map, device map, board profiles. | E1 |
| `firmware/` | Firmware metadata only (SHA-256 hashes); never firmware bytes. | E1 |
| `nvram/` | Factory-default and corrupt-checksum NVRAM JSON images. | E7 |
| `cards/` | Magnetic and smart-card payload fixtures. | E8 |
| `modem/` | Modem RX byte streams (clean connect, dropped carrier, noisy line). | E6 |
| `scenarios/` | JSON scenarios per [`../docs/scenario-runner.md`](../docs/scenario-runner.md). | E5 onward |
| `display/` | Expected VFD outputs for both variants. | E4 |

## Authoring rules

- All fixture files are committed to the repository (except firmware binaries).
- Fixtures are content-addressed by SHA-256 where applicable; tests verify hashes.
- `firmware/` may contain only the `firmware-hashes.json` registry and a `README.md` — never the firmware binaries themselves.

## Cross-references

- [`../docs/file-plan.md`](../docs/file-plan.md) — full table of fixtures.
- [`../specs/`](../specs/) — schemas validated by `tests/fixtures/`.
- [`../test-plans/`](../test-plans/) — which tests consume each fixture.
