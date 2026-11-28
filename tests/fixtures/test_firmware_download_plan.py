# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import sys

import pytest

ACCEPTANCE = Path(__file__).resolve().parents[2] / "tools/acceptance"
sys.path.insert(0, str(ACCEPTANCE))
from firmware_download_plan import (BLOCK_BYTES, IMAGE_BYTES, crc16_xmodem,
                                    registered_download_plan)


def test_registered_plan_reports_exact_changed_blocks_without_modifying_images():
    source = bytes([0x11]) * IMAGE_BYTES
    target = bytearray(source)
    target[3 * BLOCK_BYTES + 7] = 0x22
    target[191 * BLOCK_BYTES + 9] = 0x33
    target = bytes(target)
    plan = registered_download_plan(source, target)
    assert plan["kind"] == "registered-image-delta"
    assert plan["requested_blocks"] == [3, 191]
    assert plan["source_sha256"] != plan["target_sha256"]
    assert plan["block_crc16_xmodem"] == {
        "3": f"{crc16_xmodem(target[3 * BLOCK_BYTES:4 * BLOCK_BYTES]):04x}",
        "191": f"{crc16_xmodem(target[191 * BLOCK_BYTES:192 * BLOCK_BYTES]):04x}",
    }
    assert source[3 * BLOCK_BYTES + 7] == 0x11


def test_crc16_xmodem_standard_check_value():
    assert crc16_xmodem(b"123456789") == 0x31c3


@pytest.mark.parametrize("source_size,target_size", [
    (IMAGE_BYTES - 1, IMAGE_BYTES),
    (IMAGE_BYTES, IMAGE_BYTES + 1),
])
def test_registered_plan_rejects_wrong_image_sizes(source_size, target_size):
    with pytest.raises(ValueError, match="exactly 1 MiB"):
        registered_download_plan(bytes(source_size), bytes(target_size))


def test_registered_plan_rejects_identical_images():
    image = bytes(IMAGE_BYTES)
    with pytest.raises(ValueError, match="identical"):
        registered_download_plan(image, image)
