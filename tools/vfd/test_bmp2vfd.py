"""Synthetic tests for the independent graphical bitmap converter."""

import contextlib
import hashlib
import io
from pathlib import Path
import random
import re
import struct
import tempfile
import unittest

from bmp2vfd import (
    Raster, bitmap_raster, fit_raster, format_asm, format_dat, legacy_bitmap,
    main, pack_raster, preview_png, unpack_graphic,
)


def make_mono_bmp(width, height, file_rows, *, padded=True, top_down=False, reverse=False):
    row_bytes = (width + 7) // 8
    stride = ((width + 31) // 32) * 4 if padded else row_bytes
    assert all(len(row) == row_bytes for row in file_rows)
    pixels = b"".join(row + bytes(stride - row_bytes) for row in file_rows)
    offset = 62
    palette = (b"\xff\xff\xff\x00\x00\x00\x00\x00" if reverse
               else b"\x00\x00\x00\x00\xff\xff\xff\x00")
    return (struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
            + struct.pack("<IiiHHIIiiII", 40, width, -height if top_down else height,
                          1, 1, 0, len(pixels), 2835, 2835, 2, 2)
            + palette + pixels)


def synthetic_probe(padded):
    # Deterministic synthetic test data, identical to that sent to the DOS utility.
    rng = random.Random(0x4B50)
    rows = [bytes(rng.randrange(256) for _ in range(14)) for _ in range(72)]
    return make_mono_bmp(112, 72, rows, padded=padded)


class ConverterTests(unittest.TestCase):
    def test_legacy_dos_parity_standard_padded(self):
        encoded = legacy_bitmap(synthetic_probe(True))
        self.assertEqual(len(encoded.data), 1008)
        # SHA-256 of independently observed original utility output on synthetic input.
        self.assertEqual(hashlib.sha256(encoded.data).hexdigest(),
                         "a49b116de9108d52a531f56da332cc1cd67aad764b5b3d2f7780119ccec74a48")

    def test_legacy_dos_parity_tightly_packed(self):
        encoded = legacy_bitmap(synthetic_probe(False))
        self.assertEqual(hashlib.sha256(encoded.data).hexdigest(),
                         "a6d6079d02d269ac9243553a672a19abb93b0324b050b535bdf34e7a909ac510")

    def test_standard_bmp_padding_is_corrected(self):
        correct = pack_raster(bitmap_raster(synthetic_probe(True)))
        # Standard BMP decoding should equal the intended tightly packed input result.
        self.assertEqual(correct.data, legacy_bitmap(synthetic_probe(False)).data)
        self.assertNotEqual(correct.data, legacy_bitmap(synthetic_probe(True)).data)

    def test_palette_reversal_and_top_down_orientation(self):
        rows = [b"\x80", b"\x40"]
        a = bitmap_raster(make_mono_bmp(2, 2, rows, top_down=True))
        b = bitmap_raster(make_mono_bmp(2, 2, rows[::-1]))
        c = bitmap_raster(make_mono_bmp(2, 2, [bytes([x[0] ^ 0xff]) for x in rows],
                                        top_down=True, reverse=True))
        self.assertEqual(a.rows, ((0, 1), (1, 0)))
        self.assertEqual(a, b)
        self.assertEqual(a, c)
        self.assertEqual(bitmap_raster(make_mono_bmp(2, 2, rows, top_down=True), "light").rows,
                         ((1, 0), (0, 1)))

    def test_other_uncompressed_bmp_depths(self):
        for bits, row, palette in (
            (4, b"\x01\x00", b"\x00\x00\x00\x00\xff\xff\xff\x00"),
            (8, b"\x00\x01\x00", b"\x00\x00\x00\x00\xff\xff\xff\x00"),
            (24, b"\x00\x00\x00\xff\xff\xff\x00\x00\x00", b""),
            (32, b"\x00\x00\x00\x00\xff\xff\xff\xff\x00\x00\x00\x00", b""),
        ):
            with self.subTest(bits=bits):
                stride = ((3 * bits + 31) // 32) * 4
                pixels = row + bytes(stride - len(row))
                offset = 54 + len(palette)
                data = (struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
                        + struct.pack("<IiiHHIIiiII", 40, 3, 1, 1, bits, 0, len(pixels),
                                      2835, 2835, len(palette) // 4, 0)
                        + palette + pixels)
                self.assertEqual(bitmap_raster(data).rows, ((1, 0, 1),))

    def test_bottom_first_bands_and_bit_orientation(self):
        matrix = [[0] * 112 for _ in range(72)]
        for y, x in [(0, 0), (7, 1), (8, 2), (71, 3), (64, 4)]:
            matrix[y][x] = 1
        raster = Raster(112, 72, tuple(tuple(x) for x in matrix))
        g = pack_raster(raster, "git")
        expected = {8 * 112: 0x01, 8 * 112 + 1: 0x80,
                    7 * 112 + 2: 0x01, 3: 0x80, 4: 0x01}
        self.assertEqual({i: b for i, b in enumerate(g.data) if b}, expected)
        self.assertEqual(unpack_graphic(g), raster)
        self.assertEqual(g.data[-36:], bytes(36))

    def test_formats_and_graphical_filler(self):
        g = pack_raster(Raster(1, 1, ((1,),)), "git")
        self.assertEqual(len(g.data), 1044)
        self.assertEqual(g.data[-36:], bytes(36))
        asm = format_asm(g.data)
        dat = format_dat(g.data)
        self.assertTrue(dat.startswith("{\n"))
        self.assertTrue(dat.endswith("\n},\n"))
        for output in (asm, dat):
            self.assertEqual(bytes(int(n, 16) for n in re.findall(r"0x([0-9A-F]{2})", output)), g.data)

    def test_direct_page_stride_and_padding(self):
        g = pack_raster(Raster(126, 88, tuple((1,) * 126 for _ in range(88))), "page")
        self.assertEqual(len(g.data), 1408)
        for band in range(11):
            self.assertEqual(g.data[band * 128:band * 128 + 126], b"\xff" * 126)
            self.assertEqual(g.data[band * 128 + 126:band * 128 + 128], bytes(2))

    def test_centering_and_fit(self):
        small = Raster(1, 1, ((1,),))
        g = pack_raster(small, "git", center=True)
        raster = unpack_graphic(g)
        self.assertEqual(raster.rows[35][55], 1)
        large = Raster(224, 72, tuple((1,) * 224 for _ in range(72)))
        self.assertEqual((fit_raster(large, 112, 72).width, fit_raster(large, 112, 72).height), (112, 36))
        with self.assertRaisesRegex(ValueError, "use --fit"):
            pack_raster(large, "git")

    def test_bad_inputs_rejected(self):
        with self.assertRaisesRegex(ValueError, "not a Windows BMP"):
            bitmap_raster(b"not a bitmap")
        with self.assertRaisesRegex(ValueError, "truncated"):
            bitmap_raster(synthetic_probe(True)[:-5])
        with self.assertRaisesRegex(ValueError, "nonstandard packed"):
            bitmap_raster(synthetic_probe(False))
        with self.assertRaisesRegex(ValueError, "legacy mode requires"):
            legacy_bitmap(make_mono_bmp(8, 8, [b"\xff"] * 8))

    def test_cli_and_preview(self):
        with tempfile.TemporaryDirectory(prefix="bmp2vfd-test-") as temp:
            p = Path(temp)
            source = p / "synthetic.bmp"
            source.write_bytes(synthetic_probe(True))
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main([str(source), str(p / "screen.dat"),
                                       str(p / "screen.bin"), "--preview", str(p / "screen.png")]), 0)
            binary = (p / "screen.bin").read_bytes()
            self.assertEqual(binary, pack_raster(bitmap_raster(source.read_bytes()), "git").data)
            png = (p / "screen.png").read_bytes()
            self.assertTrue(png.startswith(b"\x89PNG\r\n\x1a\n"))
            self.assertEqual(struct.unpack_from(">II", png, 16), (448, 288))
            self.assertEqual(png, preview_png(pack_raster(bitmap_raster(source.read_bytes()), "git")))


if __name__ == "__main__":
    unittest.main()
