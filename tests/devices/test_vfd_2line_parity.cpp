// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic VFD hardware parity checks (see repo test-plans under test-plans/hardware).

#include "millennium_vfd_model.h"

#include "millennium_vfd_gfxfont.h"
#include "millennium_vfd_cell_utf8.h"

#include <cstring>
#include <iostream>

namespace {

constexpr std::uint8_t k_pio_vfd_selected_data = 0x00U; // CS low, A0 low
constexpr std::uint8_t k_pio_vfd_selected_cmd = 0x20U;  // CS low, A0 high
constexpr std::uint8_t k_pio_vfd_not_selected = 0x40U;   // CS high

} // namespace

int main()
{
	millennium_display_profile prof{};
	prof.rows = 2;
	prof.columns = 20;
	prof.variant = "2line";
	prof.busy_cycles_char = 10;
	prof.busy_cycles_clear = 50;
	prof.busy_cycles_cursor = 8;

	millennium_vfd_model v;
	v.configure(prof);
	v.reset();

	// TP-VFD-003-001: CS inactive — no cell change.
	v.write('A', 0U, k_pio_vfd_selected_data);
	if (v.cells()[0] != 'A') {
		std::cerr << "expected first write to land\n";
		return 1;
	}
	v.reset();
	v.write('B', 0U, k_pio_vfd_not_selected);
	if (v.cells()[0] != ' ') {
		std::cerr << "expected CS-high write to be ignored\n";
		return 1;
	}

	// TP-VFD-003-002: command register sets linear cursor; data writes glyph.
	v.reset();
	v.write(10, 0U, k_pio_vfd_selected_cmd);
	v.write('Z', 1U, k_pio_vfd_selected_data);
	if (v.cells()[10] != 'Z') {
		std::cerr << "command cursor then data failed\n";
		return 1;
	}

	// TP-VFD-003-008: 0x0C clears full buffer.
	v.reset();
	v.write('x', 0U, k_pio_vfd_selected_data);
	v.write('y', 1U, k_pio_vfd_selected_data);
	v.write(0x0c, 2U, k_pio_vfd_selected_data);
	for (char c : v.cells()) {
		if (c != ' ') {
			std::cerr << "expected full clear after 0x0C\n";
			return 1;
		}
	}

	// TP-VFD-003-004: DC3 / DC4 cursor visibility flags.
	v.reset();
	if (v.cursor_visible()) {
		std::cerr << "expected cursor off initially\n";
		return 1;
	}
	v.write(0x13, 0U, k_pio_vfd_selected_data);
	if (!v.cursor_visible()) {
		std::cerr << "DC3 should enable cursor visibility\n";
		return 1;
	}
	v.write(0x14, 1U, k_pio_vfd_selected_data);
	if (v.cursor_visible()) {
		std::cerr << "DC4 should disable cursor visibility\n";
		return 1;
	}

	// DC1 clears scroll modes; DC15 enables horizontal scroll flag.
	v.reset();
	v.write(0x12, 0U, k_pio_vfd_selected_data);
	if (!v.vertical_scroll_mode()) {
		std::cerr << "DC2 should enable vertical scroll mode\n";
		return 1;
	}
	v.write(0x11, 1U, k_pio_vfd_selected_data);
	if (v.vertical_scroll_mode() || v.horizontal_scroll_mode()) {
		std::cerr << "DC1 should clear scroll modes\n";
		return 1;
	}
	v.write(0x15, 2U, k_pio_vfd_selected_data);
	if (!v.horizontal_scroll_mode() || v.vertical_scroll_mode()) {
		std::cerr << "DC5 should select horizontal scroll only\n";
		return 1;
	}

	// DC6 + position enables blink state; DC7 cancels.
	v.reset();
	v.write(0x16, 0U, k_pio_vfd_selected_data);
	v.write(0x42, 1U, k_pio_vfd_selected_data);
	if (!v.blink_enabled() || v.blink_position_code() != 0x42U) {
		std::cerr << "DC6+param should set blink\n";
		return 1;
	}
	v.write(0x17, 2U, k_pio_vfd_selected_data);
	if (v.blink_enabled() || v.blink_position_code() != 0U) {
		std::cerr << "DC7 should cancel blink\n";
		return 1;
	}

	// 0x1A + level sets luminance (data path).
	v.reset();
	v.write(0x1a, 0U, k_pio_vfd_selected_data);
	v.write(0x55, 1U, k_pio_vfd_selected_data);
	if (v.luminance() != 0x55U) {
		std::cerr << "VFD_SET_LEVEL sequence should set luminance\n";
		return 1;
	}

	// Command-register 0x40: panel reset path (distinct from data byte 0x40 = '@').
	v.reset();
	v.write('A', 0U, k_pio_vfd_selected_data);
	v.write(0x40U, 1U, k_pio_vfd_selected_cmd);
	for (char c : v.cells()) {
		if (c != ' ') {
			std::cerr << "expected VFD_RESET (cmd 0x40) to clear display\n";
			return 1;
		}
	}
	if (v.cursor_visible() || v.vertical_scroll_mode() || v.horizontal_scroll_mode()) {
		std::cerr << "expected VFD_RESET to clear scroll/cursor-visibility state\n";
		return 1;
	}

	// Data path 0x40 must remain '@' (distinct from command reset).
	v.reset();
	v.write('@', 0U, k_pio_vfd_selected_data);
	if (v.cells()[0] != '@') {
		std::cerr << "expected data 0x40 to be printable @\n";
		return 1;
	}
	std::uint8_t const *g = millennium_vfd_gfxfont_glyph(0xA1U);
	if (g[0] != 0x20U || g[1] != 0x54U || g[2] != 0x55U || g[3] != 0x56U || g[4] != 0x78U) {
		std::cerr << "extended CGROM glyph 0xA1 mismatch vs reference glyph table\n";
		return 1;
	}

	std::string u8;
	millennium_vfd_cell_utf8::append_cell(u8, 0xa1U, false);
	if (u8 != "\xc3\xa0") {
		std::cerr << "UTF-8 row: 0xA1 should map to U+00E0 (à)\n";
		return 1;
	}
	u8.clear();
	millennium_vfd_cell_utf8::append_cell(u8, 0xa1U, true);
	if (u8 != "\xef\xbd\xa1") {
		std::cerr << "UTF-8 row: 0xA1 in katakana mode should be halfwidth ｡\n";
		return 1;
	}

	return 0;
}
