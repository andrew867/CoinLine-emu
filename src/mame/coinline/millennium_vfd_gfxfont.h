// SPDX-License-Identifier: BSD-3-Clause
// 5×7 glyph columns from `millennium_vfd_gfxfont_asm.h` (256 code points, column-major dot order).
#pragma once

#include <cstdint>

#include "millennium_vfd_gfxfont_asm.h"

#if defined(__has_include)
#if __has_include("bitmap.h")
#include "bitmap.h"
#define COINLINE_VFD_GFXFONT_HAS_BITMAP 1
#endif
#endif
#ifndef COINLINE_VFD_GFXFONT_HAS_BITMAP
#define COINLINE_VFD_GFXFONT_HAS_BITMAP 0
#endif

inline std::uint8_t const *millennium_vfd_gfxfont_glyph(std::uint8_t ch)
{
	return &millennium_vfd_gfxfont_asm[ch][0];
}

#if COINLINE_VFD_GFXFONT_HAS_BITMAP
inline void millennium_vfd_draw_dotmatrix_glyph(bitmap_rgb32 &bitmap, int cx, int cy, std::uint8_t ch, rgb_t const &color, int scale, rectangle const &clip, int max_w, int max_h)
{
	std::uint8_t const *g = millennium_vfd_gfxfont_glyph(ch);
	for (int col = 0; col < 5; ++col) {
		std::uint8_t const bits = g[col];
		for (int row = 0; row < 7; ++row) {
			if (((bits >> row) & 1) == 0)
				continue;
			for (int sy = 0; sy < scale; ++sy) {
				int const y = cy + row * scale + sy;
				if (y < clip.min_y || y > clip.max_y || y >= max_h)
					continue;
				for (int sx = 0; sx < scale; ++sx) {
					int const x = cx + col * scale + sx;
					if (x < clip.min_x || x > clip.max_x || x >= max_w)
						continue;
					bitmap.pix(y, x) = color;
				}
			}
		}
	}
}
#endif // COINLINE_VFD_GFXFONT_HAS_BITMAP
