# SPDX-License-Identifier: GPL-2.0-or-later
from pathlib import Path
import sys

import pytest

ACCEPTANCE = Path(__file__).resolve().parents[2] / "tools/acceptance"
sys.path.insert(0, str(ACCEPTANCE))
from firmware_download_fixture import BLOCK_BYTES, IMAGE_BYTES, MARKER, crc16_xmodem, synthetic_target


def test_synthetic_target_changes_only_registered_erased_block():
    seed = bytes([0x11]) * (0x80 * BLOCK_BYTES) + bytes([0xff]) * (IMAGE_BYTES - 0x80 * BLOCK_BYTES)
    target, manifest = synthetic_target(seed)
    assert target[:0x80 * BLOCK_BYTES] == seed[:0x80 * BLOCK_BYTES]
    assert target[0x80 * BLOCK_BYTES:0x80 * BLOCK_BYTES + len(MARKER)] == MARKER
    assert target[(0x80 + 1) * BLOCK_BYTES:] == seed[(0x80 + 1) * BLOCK_BYTES:]
    assert manifest["requested_blocks"] == [0x80]
    assert manifest["source_sha256"] != manifest["target_sha256"]
    block = target[0x80 * BLOCK_BYTES:(0x80 + 1) * BLOCK_BYTES]
    assert manifest["block_crc16_xmodem"] == {"128": f"{crc16_xmodem(block):04x}"}


def test_crc16_xmodem_standard_check_value():
    assert crc16_xmodem(b"123456789") == 0x31c3


@pytest.mark.parametrize("block", [0x7f, 0xd0, True])
def test_target_rejects_unsafe_block(block):
    with pytest.raises(ValueError, match="main-program block"):
        synthetic_target(bytes([0xff]) * IMAGE_BYTES, block)


def test_target_rejects_non_erased_block():
    with pytest.raises(ValueError, match="not erased"):
        synthetic_target(bytes(IMAGE_BYTES))
