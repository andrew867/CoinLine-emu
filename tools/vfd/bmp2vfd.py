#!/usr/bin/env python3
"""Convert bitmap art into Millennium graphical VFD column data.

Independent implementation based on synthetic input/output experiments.
No original converter code or original artwork is included.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import struct
import sys
import zlib


GRAPHIC_WIDTH = 112
GRAPHIC_BANDS = 9
PAGE_WIDTH = 126
PAGE_STRIDE = 128
PAGE_BANDS = 11
ROWS_PER_BAND = 8
GRAPHIC_FILLER = 36


@dataclass(frozen=True)
class Raster:
    """Top-down, left-to-right pixels; one denotes an illuminated dot."""

    width: int
    height: int
    rows: tuple[tuple[int, ...], ...]

    def __post_init__(self) -> None:
        if self.width < 1 or self.height < 1:
            raise ValueError("image dimensions must be positive")
        if len(self.rows) != self.height or any(
            len(row) != self.width for row in self.rows
        ):
            raise ValueError("raster size does not match its dimensions")
        if any(pixel not in (0, 1) for row in self.rows for pixel in row):
            raise ValueError("raster pixels must be zero or one")


@dataclass(frozen=True)
class PackedGraphic:
    data: bytes
    width: int
    stride: int
    bands: int
    filler: int = 0

    @property
    def height(self) -> int:
        return self.bands * ROWS_PER_BAND

    @property
    def bitmap(self) -> bytes:
        return self.data[:self.stride * self.bands]


@dataclass(frozen=True)
class BmpInfo:
    width: int
    height: int
    top_down: bool
    bits: int
    offset: int
    palette: tuple[tuple[int, int, int], ...]
    stride: int


def bmp_info(data: bytes) -> BmpInfo:
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("not a Windows BMP file")
    offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40 or 14 + dib_size > len(data):
        raise ValueError("requires a Windows BITMAPINFOHEADER or later")
    width, signed_height, planes, bits, compression = struct.unpack_from(
        "<iiHHI", data, 18
    )
    if width <= 0 or signed_height == 0 or planes != 1:
        raise ValueError("invalid BMP dimensions or plane count")
    height = abs(signed_height)
    if width * height > 16_000_000:
        raise ValueError("image exceeds the converter's 16 million pixel limit")
    if bits not in (1, 4, 8, 24, 32) or compression != 0:
        raise ValueError("requires uncompressed 1-, 4-, 8-, 24- or 32-bit BMP")
    if not 14 + dib_size <= offset <= len(data):
        raise ValueError("invalid BMP pixel offset")
    palette: tuple[tuple[int, int, int], ...] = ()
    if bits <= 8:
        count = struct.unpack_from("<I", data, 46)[0] or (1 << bits)
        start = 14 + dib_size
        if count > (1 << bits) or start + 4 * count > offset:
            raise ValueError("invalid or truncated BMP colour palette")
        palette = tuple(
            (data[start + 4 * i + 2], data[start + 4 * i + 1], data[start + 4 * i])
            for i in range(count)
        )
    stride = ((width * bits + 31) // 32) * 4
    return BmpInfo(width, height, signed_height < 0, bits, offset, palette, stride)


def dot_from_rgb(r: int, g: int, b: int, ink: str, threshold: int) -> int:
    # Integer weighted luminance avoids platform-specific rounding.
    dark = 299 * r + 587 * g + 114 * b < 1000 * threshold
    return int(dark if ink == "dark" else not dark)


def bitmap_raster(data: bytes, ink: str = "dark", threshold: int = 128) -> Raster:
    info = bmp_info(data)
    if info.offset + info.height * info.stride > len(data):
        raise ValueError("BMP is truncated or uses nonstandard packed scanlines; "
                         "use --legacy-row-padding only for compatible old bitmaps")
    rows = []
    for y in range(info.height):
        row_index = y if info.top_down else info.height - 1 - y
        start = info.offset + row_index * info.stride
        row = data[start:start + info.stride]
        pixels = []
        for x in range(info.width):
            if info.bits == 1:
                index = (row[x // 8] >> (7 - x % 8)) & 1
            elif info.bits == 4:
                index = (row[x // 2] >> (4 if x % 2 == 0 else 0)) & 15
            elif info.bits == 8:
                index = row[x]
            else:
                start_rgb = x * (info.bits // 8)
                b, g, r = row[start_rgb:start_rgb + 3]
                pixels.append(dot_from_rgb(r, g, b, ink, threshold))
                continue
            if index >= len(info.palette):
                raise ValueError("BMP pixel references a missing palette entry")
            pixels.append(dot_from_rgb(*info.palette[index], ink, threshold))
        rows.append(tuple(pixels))
    return Raster(info.width, info.height, tuple(rows))


def load_raster(path: Path, ink: str, threshold: int) -> Raster:
    if path.suffix.lower() == ".bmp":
        return bitmap_raster(path.read_bytes(), ink, threshold)
    try:
        from PIL import Image
    except ImportError as exc:
        raise ValueError("non-BMP inputs require Pillow; install Pillow or use BMP") from exc
    with Image.open(path) as source:
        if source.width * source.height > 16_000_000:
            raise ValueError("image exceeds the converter's 16 million pixel limit")
        # Composite transparency onto an unlit background for the requested polarity.
        bg = (255, 255, 255, 255) if ink == "dark" else (0, 0, 0, 255)
        rgba = source.convert("RGBA")
        rgb = Image.alpha_composite(Image.new("RGBA", rgba.size, bg), rgba).convert("RGB")
        width, height = rgb.size
        pix = rgb.load()
        rows = tuple(tuple(dot_from_rgb(*pix[x, y], ink, threshold)
                           for x in range(width)) for y in range(height))
    return Raster(width, height, rows)


def fit_raster(raster: Raster, max_width: int, max_height: int) -> Raster:
    if raster.width <= max_width and raster.height <= max_height:
        return raster
    # Downscale only; use integer dimensions and nearest-neighbour sampling.
    if raster.width * max_height >= raster.height * max_width:
        width = max_width
        height = max(1, raster.height * max_width // raster.width)
    else:
        height = max_height
        width = max(1, raster.width * max_height // raster.height)
    rows = tuple(tuple(raster.rows[min(raster.height - 1, (2 * y + 1) * raster.height // (2 * height))]
                       [min(raster.width - 1, (2 * x + 1) * raster.width // (2 * width))]
                       for x in range(width)) for y in range(height))
    return Raster(width, height, rows)


def profile_geometry(profile: str, image_height: int) -> tuple[int, int, int, int]:
    if profile == "git":
        return GRAPHIC_WIDTH, GRAPHIC_WIDTH, GRAPHIC_BANDS, GRAPHIC_FILLER
    if profile == "page":
        return PAGE_WIDTH, PAGE_STRIDE, PAGE_BANDS, 0
    if profile == "bitmap":
        bands = min(PAGE_BANDS, (image_height + ROWS_PER_BAND - 1) // ROWS_PER_BAND)
        return GRAPHIC_WIDTH, GRAPHIC_WIDTH, bands, 0
    raise ValueError(f"unknown graphic profile: {profile}")


def pack_raster(raster: Raster, profile: str = "bitmap", *, center: bool = False) -> PackedGraphic:
    width, stride, bands, filler = profile_geometry(profile, raster.height)
    height = bands * ROWS_PER_BAND
    if raster.width > width or raster.height > height:
        raise ValueError(f"image is {raster.width}x{raster.height}; profile allows "
                         f"at most {width}x{height}; use --fit to scale down")
    x0 = (width - raster.width) // 2 if center else 0
    y0 = (height - raster.height) // 2 if center else 0
    payload = bytearray(stride * bands + filler)
    for y, row in enumerate(raster.rows):
        from_bottom = height - 1 - (y0 + y)
        band, dot_from_bottom = divmod(from_bottom, ROWS_PER_BAND)
        bit = 1 << (7 - dot_from_bottom)
        for x, pixel in enumerate(row):
            if pixel:
                payload[band * stride + x0 + x] |= bit
    return PackedGraphic(bytes(payload), width, stride, bands, filler)


def legacy_bitmap(data: bytes, profile: str = "bitmap") -> PackedGraphic:
    """Reproduce observed historical raw index/scanline behaviour.

    Limited to the well-established 112-column, bottom-up one-bit input
    and complete eight-dot bands. Other historical inputs produced unreliable
    results and are deliberately handled by the regular converter instead.
    """
    info = bmp_info(data)
    if (info.bits != 1 or info.top_down or info.width != GRAPHIC_WIDTH
            or info.height % ROWS_PER_BAND or info.height > PAGE_BANDS * ROWS_PER_BAND):
        raise ValueError("legacy mode requires bottom-up 112-column 1-bit BMP "
                         "with a positive height divisible by eight, at most 88")
    if profile not in ("bitmap", "git"):
        raise ValueError("legacy mode does not support a directly addressed page")
    if profile == "git" and info.height != GRAPHIC_BANDS * ROWS_PER_BAND:
        raise ValueError("legacy GIT mode requires exactly 112x72 pixels")
    source_stride = GRAPHIC_WIDTH // 8  # Historical reader ignores DWORD padding.
    if info.offset + info.height * source_stride > len(data):
        raise ValueError("legacy bitmap input is truncated")
    bands = info.height // ROWS_PER_BAND
    filler = GRAPHIC_FILLER if profile == "git" else 0
    output = bytearray(GRAPHIC_WIDTH * bands + filler)
    for band in range(bands):
        for x in range(GRAPHIC_WIDTH):
            value = 0
            for dot in range(ROWS_PER_BAND):
                at = info.offset + (band * ROWS_PER_BAND + dot) * source_stride + x // 8
                pixel = ((data[at] >> (7 - x % 8)) & 1) ^ 1
                value |= pixel << (7 - dot)
            output[band * GRAPHIC_WIDTH + x] = value
    return PackedGraphic(bytes(output), GRAPHIC_WIDTH, GRAPHIC_WIDTH, bands, filler)


def unpack_graphic(graphic: PackedGraphic) -> Raster:
    rows = []
    for y in range(graphic.height):
        band, dot = divmod(graphic.height - 1 - y, ROWS_PER_BAND)
        rows.append(tuple((graphic.data[band * graphic.stride + x] >> (7 - dot)) & 1
                          for x in range(graphic.width)))
    return Raster(graphic.width, graphic.height, tuple(rows))


def format_asm(data: bytes) -> str:
    return "".join("\t .DB\t" + ",".join(f"0x{x:02X}" for x in data[i:i + 8]) + "\n"
                   for i in range(0, len(data), 8))


def format_dat(data: bytes) -> str:
    lines = ["    " + ", ".join(f"0x{x:02X}" for x in data[i:i + 16])
             for i in range(0, len(data), 16)]
    return "{\n" + ",\n".join(lines) + "\n},\n"


def preview_png(graphic: PackedGraphic, scale: int = 4) -> bytes:
    if not 1 <= scale <= 16:
        raise ValueError("preview scale must be between 1 and 16")
    raster = unpack_graphic(graphic)
    width, height = raster.width * scale, raster.height * scale
    pixels = bytearray()
    on, off = bytes((91, 238, 204)), bytes((7, 19, 21))
    for row in raster.rows:
        expanded = b"\x00" + b"".join((on if point else off) * scale for point in row)
        pixels.extend(expanded * scale)

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(pixels, 9)) + chunk(b"IEND", b""))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="BMP, or another image format via optional Pillow")
    parser.add_argument("output", type=Path, help="output .asm, .bin or .dat file")
    parser.add_argument("binary", type=Path, nargs="?", help="optional second raw binary output (legacy CLI form)")
    parser.add_argument("--format", choices=("asm", "bin", "dat"), help="override output format inferred from extension")
    parser.add_argument("--profile", choices=("bitmap", "git", "page"),
                        help="bitmap: 112 columns; git: 112x72 plus 36 filler bytes; page: 128-byte stride, 11 bands")
    parser.add_argument("--ink", choices=("dark", "light"), default="dark", help="which source colour becomes illuminated")
    parser.add_argument("--threshold", type=int, default=128, help="luminance threshold, 0 through 255")
    parser.add_argument("--fit", action="store_true", help="downscale oversized images proportionally")
    parser.add_argument("--center", action="store_true", help="center image in the output canvas instead of top-left")
    parser.add_argument("--legacy-row-padding", action="store_true", help="reproduce observed old mono-BMP reader, including incorrect scanline padding")
    parser.add_argument("--preview", type=Path, help="write a PNG preview of the actual packed data")
    parser.add_argument("--preview-scale", type=int, default=4, help="integer preview magnification, 1 through 16")
    args = parser.parse_args(argv)
    form = args.format or {".asm": "asm", ".bin": "bin", ".dat": "dat"}.get(args.output.suffix.lower())
    if form is None:
        parser.error("specify --format or use .asm, .bin or .dat")
    profile = args.profile or ("git" if form == "dat" else "bitmap")
    if form == "dat" and profile != "git":
        parser.error(".dat graphical screen fragments require the 1044-byte git profile")
    if not 0 <= args.threshold <= 255:
        parser.error("threshold must be between 0 and 255")
    if not 1 <= args.preview_scale <= 16:
        parser.error("preview scale must be between 1 and 16")
    targets = [p.resolve() for p in (args.input, args.output, args.binary, args.preview) if p is not None]
    if len(set(targets)) != len(targets):
        parser.error("input, formatted output, binary output and preview must be different files")
    try:
        if args.legacy_row_padding:
            if args.ink != "dark" or args.threshold != 128 or args.fit or args.center:
                raise ValueError("legacy mode uses raw palette indices and does not allow ink, threshold, fit or center changes")
            graphic = legacy_bitmap(args.input.read_bytes(), profile)
        else:
            raster = load_raster(args.input, args.ink, args.threshold)
            if args.fit:
                max_width, _, bands, _ = profile_geometry(profile, raster.height)
                raster = fit_raster(raster, max_width, bands * ROWS_PER_BAND)
            graphic = pack_raster(raster, profile, center=args.center)
        if form == "bin":
            result = graphic.data
        else:
            result = (format_asm(graphic.data) if form == "asm" else format_dat(graphic.data)).encode("ascii")
        preview = preview_png(graphic, args.preview_scale) if args.preview is not None else None
        args.output.write_bytes(result)
        if args.binary:
            args.binary.write_bytes(graphic.data)
        if args.preview and preview is not None:
            args.preview.write_bytes(preview)
    except (OSError, ValueError) as exc:
        print(f"bmp2vfd: {exc}", file=sys.stderr)
        return 2
    print(f"{graphic.width}x{graphic.height} pixels, {graphic.bands} bands, "
          f"{graphic.stride} bytes/row band, {len(graphic.data)} output bytes "
          f"({graphic.filler} filler): {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
