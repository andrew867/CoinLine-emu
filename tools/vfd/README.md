# BMP to graphical VFD converter

Independent Python replacement for the historical DOS bitmap conversion utility. Normal output fixes an observed scanline-padding problem in the original utility, and an explicit legacy mode reproduces its observed behavior on supported inputs. No proprietary converter code, original images or binary output are included.

## Quick start

Use Python 3.10 or later. BMP input and PNG preview output require only the standard library. Other input formats, including PNG and JPEG, can be opened if Pillow is installed.

```bash
python tools/vfd/bmp2vfd.py artwork.bmp artwork.asm artwork.bin
python tools/vfd/bmp2vfd.py screen.bmp screen.dat screen.bin --preview screen.png
python tools/vfd/bmp2vfd.py logo.png logo.dat --fit --center --preview logo-preview.png
python tools/vfd/bmp2vfd.py screen.bmp display-page.bin --profile page --ink light
```

The first example accepts the original utility's three positional arguments. The binary output is optional. An output filename ending in `.asm` produces assembly byte declarations; `.bin` produces raw bytes; `.dat` produces a complete C-array initializer fragment with its trailing comma. The `.dat` default is the 1044-byte graphical-screen record profile.

## Image profiles

| Profile | Source canvas | Output | Use |
| --- | --- | --- | --- |
| `bitmap` | Up to 112 columns, 1 to 88 rows | 112 bytes per band of eight source rows | Raw bitmap or assembly data, with the height rounded up to a complete band |
| `git` | Up to 112 × 72 | 1008 bitmap bytes + 36 zero filler bytes = 1044 | Complete graphical screen bitmap payload |
| `page` | Up to 126 × 88 | 128-byte stride × 11 bands = 1408 bytes | Candidate raw large-display backing-page representation, with two blank padding columns |

Smaller sources are placed at the top left of the selected canvas and unused space is unlit. Use `--center` for centered art. Oversized images are rejected unless `--fit` is supplied; fitting preserves aspect ratio and downsamples with nearest-neighbour sampling. The 88-dot full-page raster is a logical representation based on eight bits per column byte, not a verified physical panel pitch specification.

By default dark source pixels become illuminated dots, matching the original converter with a normal black/white palette. Use `--ink light` to illuminate white or light-coloured pixels on a dark background. `--threshold` sets the weighted luminance cutoff. Standard Windows BMP input supports uncompressed 1-, 4-, 8-, 24- and 32-bit forms, both bottom-up and top-down, with normal DWORD scanline padding. Palette entries are interpreted by their colour, not simply their numeric index.

The optional PNG preview renders the **actual packed output**, not the source image. It is useful for catching orientation errors, incorrect polarity, clipping or legacy-padding artifacts before including the image in a build. Preview colour and scaling are illustrative, not a calibrated optical measurement.

## Verified legacy format and compatibility option

The historical DOS utility was exercised on synthetic asymmetric and deterministic random 112×72 one-bit images, in a separate isolated emulator instance. Its help describes an input and assembly output, but an additional third argument is required for its binary output. The binary output and assembly byte declarations agreed byte-for-byte for the tested images.

Observed encoding:

1. The graphic is stored left to right within each band and bottom band first.
2. Each column byte packs eight vertical source pixels. The top source pixel within a band occupies bit 0, and the bottom source pixel occupies bit 7.
3. A zero source palette index produces an illuminated bit. The original uses numeric palette indices rather than modern luminance interpretation in the verified conversion cases.
4. For a 112-column one-bit image, the old reader consumes 14 source bytes per row, even though a standard Windows BMP pads that row to 16 bytes. This shifts the intended content from successive rows.
5. The original produces 112 bytes per complete eight-row band, with no automatic 36-byte graphical-record filler. Archived graphical screen samples have 1008 bitmap bytes followed by 36 zero bytes.

For matching historical output exactly on a bottom-up one-bit 112-column BMP with whole eight-row bands, use:

```bash
python tools/vfd/bmp2vfd.py old.bmp old.asm old.bin --legacy-row-padding
```

The legacy mode intentionally reproduces the old row-padding and index semantics. It is not recommended for creating new screen assets. Inputs outside the verified layout are rejected in this mode rather than attempting to reproduce unreliable behavior of the old binary. The normal mode correctly decodes standard BMP row padding; `--profile git` also adds the 36-byte filler required for complete graphical-screen records.

Two deterministic synthetic probes, one with standard padding and one with tightly packed scanlines, produced byte-identical binary data between the historical utility and the Python legacy mode. Their output hashes are part of the test suite, so regression checks do not require the historic executable. The regular converter also matched the intended raster of the tightly packed probe when decoding the standards-compliant BMP.

## Test

From the emulator repository directory:

```bash
python -m unittest discover -s tools/vfd -p 'test_*.py'
```

The suite checks legacy output fingerprints, standard BMP row padding, palette orientation, band and bit order, graphical-record filler, direct-page stride, preview creation, input validation and the command-line conversion path. Actual rendering on a physical graphical terminal remains a separate hardware acceptance test.

The [11-line VFD interaction specification](../../specs/hardware/11-line-vfd-black-box-handoff.md) describes the corresponding display and input interface.
