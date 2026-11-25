# SPDX-License-Identifier: GPL-2.0-or-later
"""Hardware presets shared by the interactive launcher and acceptance runner."""

from dataclasses import dataclass
import re


@dataclass(frozen=True)
class Hardware:
    motherboard: str
    family: str
    revision: str
    card_dma_channel: int = 0


PRESETS = {
    "multipay-v1": Hardware("earlier", "Multi-Pay", "1.0"),
    "multipay-v1-dual-flash": Hardware("later", "Multi-Pay", "1.0"),
    "multipay-v1.1": Hardware("later", "Multi-Pay", "1.1", 1),
    "multipay-v1.3": Hardware("later", "Multi-Pay", "1.3", 1),
    "desk-rel1": Hardware("earlier", "Desk", "REL 1"),
    "desk-rel2": Hardware("earlier", "Desk", "REL 2"),
    "card-only": Hardware("earlier", "Card-only", ""),
    "coin-basic": Hardware("earlier", "Coin Basic", ""),
}

# REL in Desk family names refers to the terminal configuration. A Desk REL 2
# program can still contain a V1.0 program tag.
FIRMWARE_PRESETS = {
    "06JAA03": "multipay-v1", "12AAV08": "multipay-v1", "M06CAA17": "multipay-v1",
    "NAA1S05": "multipay-v1", "NBA1F02": "multipay-v1", "NNA1F02": "multipay-v1",
    "NNK1F05": "multipay-v1", "NPA1F01": "multipay-v1", "NPA1S01": "multipay-v1",
    "FREETEL-1.x": "multipay-v1", "NCA1X03": "multipay-v1-dual-flash",
    "NKA1X02": "multipay-v1.1", "NKA1X05": "multipay-v1.1", "NLA1X05": "multipay-v1.3",
    "NOA1X04": "multipay-v1.3", "NOK1X03": "multipay-v1.3", "NQA1X01": "multipay-v1.3",
    # Validated later wiring; its program tag's 1.3 identifies the DLA.
    "NLA1X08": "multipay-v1.3",
    "NBJ1S03": "desk-rel1", "NBJ1F03": "desk-rel1", "NAJ2S05": "desk-rel2",
    "M06CBBX1": "card-only", "NAD4K02": "coin-basic", "NAD4S02": "coin-basic",
    "NPE1S01": "coin-basic",
}

PROGRAM_TAG = re.compile(rb"<([A-Z0-9]{7,8})V(\d+\.\d+)")


def program_tag(image):
    tags = {(code.decode(), revision.decode()) for code, revision in PROGRAM_TAG.findall(image)}
    return next(iter(tags)) if len(tags) == 1 else (None, None)


def select_hardware(requested, image, verified_id=None):
    if requested != "auto":
        if requested not in PRESETS:
            raise ValueError(f"Unknown hardware preset {requested!r}. Choose auto or {', '.join(PRESETS)}.")
        return requested, "manual"
    tagged_id, revision = program_tag(image)
    if verified_id is not None and verified_id in FIRMWARE_PRESETS:
        return FIRMWARE_PRESETS[verified_id], "verified inventory"
    preset = FIRMWARE_PRESETS.get(tagged_id)
    if preset and (not PRESETS[preset].revision or not PRESETS[preset].revision[0].isdigit()
                   or PRESETS[preset].revision == revision):
        return preset, "program tag"
    return None, "firmware hash registry"
