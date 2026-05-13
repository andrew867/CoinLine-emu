# Millennium Layout Generator

Regenerates `millennium.lay` bounds directly from the color reference image.

## Usage

From repo root:

`python tools/layout/generate_millennium_layout.py`

Optional arguments:

- `--reference <path>`: alternate color-overlay image
- `--output <path1> <path2> ...`: write one or more layout files

Default outputs:

- `src/mame/layout/millennium.lay`
- `artwork/millennium.lay`
