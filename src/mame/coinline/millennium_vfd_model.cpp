// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_vfd_model.h"
#include "millennium_vfd_gfxfont.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>
#include <vector>

namespace {

std::string json_escape(std::string const &s)
{
	std::ostringstream o;
	for (char c : s) {
		if (c == '\\' || c == '"')
			o << '\\' << c;
		else if (static_cast<unsigned char>(c) < 0x20) {
			char buf[8];
			std::snprintf(buf, sizeof(buf), "\\u%04x", int(static_cast<unsigned char>(c)));
			o << buf;
		} else
			o << c;
	}
	return o.str();
}

std::string raw_hex_spaced(std::vector<std::uint8_t> const &raw)
{
	std::ostringstream o;
	for (std::size_t i = 0; i < raw.size(); ++i) {
		if (i)
			o << ' ';
		char buf[8];
		std::snprintf(buf, sizeof(buf), "0x%02X", unsigned(raw[i]));
		o << buf;
	}
	return o.str();
}

} // namespace

void millennium_vfd_model::configure(millennium_display_profile const &profile)
{
	m_profile = profile;
	if (m_profile.rows < 1)
		m_profile.rows = 1;
	if (m_profile.columns < 1)
		m_profile.columns = 1;
	if (m_profile.busy_cycles_char < 1)
		m_profile.busy_cycles_char = 1;
	if (m_profile.busy_cycles_clear < 1)
		m_profile.busy_cycles_clear = 1;
	if (m_profile.busy_cycles_cursor < 1)
		m_profile.busy_cycles_cursor = 1;
}

void millennium_vfd_model::reset()
{
	m_cells.assign(std::size_t(m_profile.rows * m_profile.columns), ' ');
	m_dot_plane.assign(std::size_t(m_profile.rows * 7 * m_profile.columns * 5), 0U);
	m_decimal_point.assign(std::size_t(m_profile.rows * m_profile.columns), 0U);
	m_comma_tail.assign(std::size_t(m_profile.rows * m_profile.columns), 0U);
	m_arrow_mark.assign(std::size_t(m_profile.rows * m_profile.columns), 0U);
	for (auto &g : m_user_glyph)
		g = { 0U, 0U, 0U, 0U, 0U };
	m_user_glyph_valid.fill(0U);
	m_raw_log.clear();
	m_cursor_row = 0;
	m_cursor_col = 0;
	m_busy_until_cycle = 0;
	m_mode = parse_mode::normal;
	m_saw_decoded_command = false;
	m_saw_printable_data = false;
	m_m6_met = false;
	m_last_unknown_escape = false;
	m_increment_write_mode = true;
	m_vertical_scroll_mode = false;
	m_horizontal_scroll_mode = false;
	m_cursor_visible = false;
	m_active_line = 0;
	m_active_page = 0U;
	m_blink_enabled = false;
	m_blink_position_code = 0U;
	m_softkey_labels.fill(std::string{});
	m_katakana_font = false;
	m_flickerless_write = false;
	m_luminance = 0xffU;
	m_cursor_blink_speed = 0U;
	m_extended_control_mode = 0U;
	m_pending_esc_cmd = 0;
	m_pending_esc_params = 0;
	m_pending_esc_args.clear();
	clear_all_dots();
}

void millennium_vfd_model::set_active_line(int line) noexcept
{
	m_active_line = std::clamp(line, 0, std::max(0, m_profile.rows - 1));
}

void millennium_vfd_model::set_active_page(std::uint8_t page) noexcept
{
	m_active_page = static_cast<std::uint8_t>(page & 0x03U);
}

void millennium_vfd_model::set_blink_enabled(bool enabled) noexcept
{
	m_blink_enabled = enabled;
}

void millennium_vfd_model::set_softkey_label(std::size_t idx, std::string const &label)
{
	if (idx >= m_softkey_labels.size())
		return;
	m_softkey_labels[idx] = label;
}

int millennium_vfd_model::cell_index() const noexcept
{
	return m_cursor_row * m_profile.columns + m_cursor_col;
}

void millennium_vfd_model::set_busy(std::uint64_t cycle, int kind)
{
	std::uint64_t const add = [&]() -> std::uint64_t {
		switch (kind) {
		case 1: return std::uint64_t(m_profile.busy_cycles_clear);
		case 2: return std::uint64_t(m_profile.busy_cycles_cursor);
		default: return std::uint64_t(m_profile.busy_cycles_char);
		}
	}();
	m_busy_until_cycle = std::max(m_busy_until_cycle, cycle) + add;
}

void millennium_vfd_model::append_raw(std::uint8_t b)
{
	m_raw_log.push_back(b);
}

void millennium_vfd_model::mark_decoded_command()
{
	m_saw_decoded_command = true;
}

void millennium_vfd_model::try_finalize_m6()
{
	bool nonempty = false;
	for (char c : m_cells) {
		if (c != ' ') {
			nonempty = true;
			break;
		}
	}
	if (nonempty && (m_saw_decoded_command || m_saw_printable_data))
		m_m6_met = true;
}

void millennium_vfd_model::clear_display()
{
	std::fill(m_cells.begin(), m_cells.end(), ' ');
	std::fill(m_decimal_point.begin(), m_decimal_point.end(), 0U);
	std::fill(m_comma_tail.begin(), m_comma_tail.end(), 0U);
	std::fill(m_arrow_mark.begin(), m_arrow_mark.end(), 0U);
	m_user_glyph_valid.fill(0U);
	clear_all_dots();
	m_cursor_row = 0;
	m_cursor_col = 0;
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::home_cursor()
{
	m_cursor_row = 0;
	m_cursor_col = 0;
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::carriage_return()
{
	m_cursor_col = 0;
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::line_feed()
{
	if (m_cursor_row + 1 < m_profile.rows) {
		++m_cursor_row;
	} else {
		// On 2x20 firmware flow, LF at bottom should push display up.
		for (int r = 0; r < m_profile.rows - 1; ++r) {
			for (int c = 0; c < m_profile.columns; ++c)
				m_cells[std::size_t(r * m_profile.columns + c)] = m_cells[std::size_t((r + 1) * m_profile.columns + c)];
		}
		for (int c = 0; c < m_profile.columns; ++c)
			m_cells[std::size_t((m_profile.rows - 1) * m_profile.columns + c)] = ' ';
		for (int r = 0; r < m_profile.rows; ++r)
			for (int c = 0; c < m_profile.columns; ++c)
				rebuild_cell_dots(r, c);
		m_cursor_row = m_profile.rows - 1;
	}
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::backspace()
{
	if (m_cursor_col > 0)
		--m_cursor_col;
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::horizontal_tab()
{
	int const tab = 4;
	int next = ((m_cursor_col / tab) + 1) * tab;
	if (next >= m_profile.columns)
		next = m_profile.columns - 1;
	m_cursor_col = std::max(0, next);
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::software_reset()
{
	clear_display();
	m_increment_write_mode = true;
	m_vertical_scroll_mode = false;
	m_horizontal_scroll_mode = false;
	m_cursor_visible = false;
	m_blink_enabled = false;
	m_blink_position_code = 0U;
	m_katakana_font = false;
	m_flickerless_write = false;
	m_pending_esc_cmd = 0;
	m_pending_esc_params = 0;
	m_extended_control_mode = 0U;
	m_pending_esc_args.clear();
	mark_decoded_command();
	try_finalize_m6();
}

void millennium_vfd_model::write_glyph(char ch)
{
	write_glyph_byte(static_cast<std::uint8_t>(ch));
}

void millennium_vfd_model::write_glyph_byte(std::uint8_t b)
{
	if (m_cursor_row < 0 || m_cursor_row >= m_profile.rows || m_cursor_col < 0
		|| m_cursor_col >= m_profile.columns)
		return;
	if (m_horizontal_scroll_mode && m_cursor_col == m_profile.columns - 1) {
		int const row = m_cursor_row;
		for (int c = 0; c < m_profile.columns - 1; ++c)
			m_cells[std::size_t(row * m_profile.columns + c)] = m_cells[std::size_t(row * m_profile.columns + c + 1)];
		m_cells[std::size_t(row * m_profile.columns + (m_profile.columns - 1))] = static_cast<char>(b);
		for (int c = 0; c < m_profile.columns; ++c)
			rebuild_cell_dots(row, c);
		m_saw_printable_data = true;
		try_finalize_m6();
		return;
	}
	m_cells[std::size_t(cell_index())] = static_cast<char>(b);
	rebuild_cell_dots(m_cursor_row, m_cursor_col);
	m_saw_printable_data = true;
	if (m_increment_write_mode) {
		if (m_cursor_col + 1 < m_profile.columns) {
			++m_cursor_col;
		} else {
			m_cursor_col = 0;
			if (m_cursor_row + 1 < m_profile.rows) {
				++m_cursor_row;
			} else if (m_vertical_scroll_mode) {
				for (int r = 0; r < m_profile.rows - 1; ++r) {
					for (int c = 0; c < m_profile.columns; ++c)
						m_cells[std::size_t(r * m_profile.columns + c)] = m_cells[std::size_t((r + 1) * m_profile.columns + c)];
				}
				for (int c = 0; c < m_profile.columns; ++c)
					m_cells[std::size_t((m_profile.rows - 1) * m_profile.columns + c)] = ' ';
				for (int r = 0; r < m_profile.rows; ++r)
					for (int c = 0; c < m_profile.columns; ++c)
						rebuild_cell_dots(r, c);
				m_cursor_row = m_profile.rows - 1;
			} else {
				m_cursor_row = 0;
			}
		}
	} else {
		if (m_cursor_col > 0) {
			--m_cursor_col;
		} else {
			m_cursor_col = m_profile.columns - 1;
			if (m_cursor_row > 0)
				--m_cursor_row;
		}
	}
	try_finalize_m6();
}

void millennium_vfd_model::log_unknown_control(std::uint8_t b)
{
	(void)b;
	m_last_unknown_escape = false;
}

void millennium_vfd_model::log_unknown_escape(std::uint8_t b2)
{
	(void)b2;
	m_last_unknown_escape = true;
}

void millennium_vfd_model::set_dot(int row, int col, bool on)
{
	if (row < 0 || row >= m_profile.rows * 7 || col < 0 || col >= m_profile.columns * 5)
		return;
	m_dot_plane[std::size_t(row * (m_profile.columns * 5) + col)] = on ? 1U : 0U;
}

void millennium_vfd_model::clear_all_dots()
{
	std::fill(m_dot_plane.begin(), m_dot_plane.end(), 0U);
}

void millennium_vfd_model::rebuild_cell_dots(int row, int col)
{
	if (row < 0 || row >= m_profile.rows || col < 0 || col >= m_profile.columns)
		return;
	std::uint8_t const ch = static_cast<std::uint8_t>(m_cells[std::size_t(row * m_profile.columns + col)]);
	std::uint8_t const *g = nullptr;
	std::size_t const idx = std::size_t(row * m_profile.columns + col);
	if (m_user_glyph_valid[ch]) {
		g = m_user_glyph[ch].data();
	} else {
		g = millennium_vfd_gfxfont_glyph(ch);
	}
	int const y0 = row * 7;
	int const x0 = col * 5;
	for (int ry = 0; ry < 7; ++ry) {
		for (int cx = 0; cx < 5; ++cx) {
			bool on = ((g[cx] >> ry) & 1U) != 0U;
			set_dot(y0 + ry, x0 + cx, on);
		}
	}
	if (m_decimal_point[idx])
		set_dot(y0 + 6, x0 + 4, true);
	if (m_comma_tail[idx]) {
		set_dot(y0 + 6, x0 + 4, true);
		set_dot(y0 + 5, x0 + 3, true);
	}
	if (m_arrow_mark[idx] && row == (m_profile.rows - 1)) {
		set_dot(y0 + 3, x0 + 4, true);
		set_dot(y0 + 2, x0 + 3, true);
		set_dot(y0 + 4, x0 + 3, true);
	}
}

std::uint8_t millennium_vfd_model::status_read(std::uint64_t cycle) const
{
	return (cycle < m_busy_until_cycle) ? 0x80U : 0x00U;
}

void millennium_vfd_model::write(std::uint8_t data, std::uint64_t cycle)
{
	write(data, cycle, 0U);
}

void millennium_vfd_model::write(std::uint8_t data, std::uint64_t cycle, std::uint8_t pio_port_b)
{
	// VFD_CSB active low: bit 6 clear selects the VFD on shared 0x60 decode.
	constexpr std::uint8_t k_vfd_csb = 0x40U;
	constexpr std::uint8_t k_vfda0 = 0x20U;
	if ((pio_port_b & k_vfd_csb) != 0U)
		return;
	if ((pio_port_b & k_vfda0) != 0U) {
		write_command_byte(data, cycle);
		return;
	}
	write_data_byte(data, cycle);
}

void millennium_vfd_model::write_command_byte(std::uint8_t cmd, std::uint64_t cycle)
{
	append_raw(cmd);
	m_last_unknown_escape = false;
	// Command register access abandons a partial data-path escape sequence.
	if (m_mode != parse_mode::normal) {
		m_mode = parse_mode::normal;
		m_pending_esc_params = 0;
		m_pending_esc_cmd = 0;
		m_pending_esc_args.clear();
	}
	// 0x40 on command register (A0=1) is panel reset; 0x40 on the data path is printable '@'.
	if (cmd == 0x40U) {
		software_reset();
		set_busy(cycle, 1);
		return;
	}
	int const max_cells = m_profile.rows * m_profile.columns;
	int const pos = std::clamp(int(cmd), 0, std::max(1, max_cells) - 1);
	m_cursor_row = pos / m_profile.columns;
	m_cursor_col = pos % m_profile.columns;
	mark_decoded_command();
	try_finalize_m6();
	set_busy(cycle, 2);
}

void millennium_vfd_model::write_data_byte(std::uint8_t data, std::uint64_t cycle)
{
	append_raw(data);
	m_last_unknown_escape = false;

	auto handle_normal = [&](std::uint8_t b) {
		if (b == 0x1b) {
			m_mode = parse_mode::esc;
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x0e) {
			clear_display();
			set_busy(cycle, 1);
			return;
		}
		if (b == 0x0c) {
			clear_display();
			set_busy(cycle, 1);
			return;
		}
		if (b == 0x08 || b == 0x09) {
			if (b == 0x08)
				backspace();
			else
				horizontal_tab();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x11) {
			// NORMAL_MODE (DC1): exit scroll modes; incremental write.
			m_increment_write_mode = true;
			m_vertical_scroll_mode = false;
			m_horizontal_scroll_mode = false;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x12) {
			m_vertical_scroll_mode = true;
			m_horizontal_scroll_mode = false;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x13) {
			m_cursor_visible = true;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x14) {
			m_cursor_visible = false;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x15) {
			m_horizontal_scroll_mode = true;
			m_vertical_scroll_mode = false;
			m_increment_write_mode = true;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x16) {
			// BLINKING_MODE (DC6): position code follows on next byte.
			m_pending_esc_cmd = 0x16U;
			m_pending_esc_params = 1U;
			m_pending_esc_args.clear();
			m_mode = parse_mode::esc_param_n;
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x17) {
			// CANCEL_BLINKING_MODE (DC7).
			m_blink_enabled = false;
			m_blink_position_code = 0U;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x10) {
			m_increment_write_mode = false;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x18 || b == 0x19) {
			std::size_t const idx = std::size_t(cell_index());
			if (idx < m_decimal_point.size()) {
				if (b == 0x18)
					m_comma_tail[idx] = 1U;
				if (b == 0x19 && m_cursor_row == (m_profile.rows - 1))
					m_arrow_mark[idx] = 1U;
				rebuild_cell_dots(m_cursor_row, m_cursor_col);
			}
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x1a) {
			// VFD_SET_LEVEL: luminance byte follows on next byte.
			m_pending_esc_cmd = 0x1aU;
			m_pending_esc_params = 1U;
			m_pending_esc_args.clear();
			m_mode = parse_mode::esc_param_n;
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x1d || b == 0x1e || b == 0x1f) {
			if (b == 0x1d)
				m_katakana_font = false;
			if (b == 0x1e)
				m_katakana_font = true;
			if (b == 0x1f)
				m_extended_control_mode = 0x1f;
			mark_decoded_command();
			try_finalize_m6();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x0d) {
			carriage_return();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x0a) {
			line_feed();
			set_busy(cycle, 2);
			return;
		}
		if (b == 0x1c) {
			m_pending_esc_cmd = 0x1cU;
			m_pending_esc_params = 6U;
			m_pending_esc_args.clear();
			m_mode = parse_mode::esc_param_n;
			set_busy(cycle, 2);
			return;
		}
		if (b >= 0x20) {
			write_glyph_byte(b);
			set_busy(cycle, 0);
			return;
		}
		log_unknown_control(b);
		set_busy(cycle, 2);
	};

	switch (m_mode) {
	case parse_mode::normal:
		handle_normal(data);
		break;

	case parse_mode::esc: {
		std::uint8_t const b2 = data & 0x7fU;
		if (b2 == 0x40 || b2 == 0x02) {
			clear_display();
			set_busy(cycle, 1);
			m_mode = parse_mode::normal;
		} else if (b2 == 0x01) {
			home_cursor();
			set_busy(cycle, 2);
			m_mode = parse_mode::normal;
		} else if (b2 == 0x48) {
			m_pending_esc_cmd = 0x48U;
			m_pending_esc_params = 1U;
			m_pending_esc_args.clear();
			m_mode = parse_mode::esc_param_n;
			set_busy(cycle, 2);
		} else if (b2 == 0x49) {
			software_reset();
			m_mode = parse_mode::normal;
			set_busy(cycle, 1);
		} else if (b2 == 0x53) {
			m_flickerless_write = true;
			mark_decoded_command();
			try_finalize_m6();
			m_mode = parse_mode::normal;
			set_busy(cycle, 2);
		} else if (b2 == 0x38 || b2 == 0x39) {
			m_extended_control_mode = b2;
			m_pending_esc_cmd = b2;
			m_pending_esc_params = 5U;
			m_pending_esc_args.clear();
			m_mode = parse_mode::esc_param_n;
			set_busy(cycle, 2);
		} else if (b2 == 0x58 || b2 == 0x59) {
			m_extended_control_mode = b2;
			mark_decoded_command();
			try_finalize_m6();
			m_mode = parse_mode::normal;
			set_busy(cycle, 2);
		} else if (b2 == 0x4c || b2 == 0x54 || b2 == 0x43) {
			m_pending_esc_cmd = b2;
			m_pending_esc_params = (b2 == 0x43) ? 2U : 1U;
			m_pending_esc_args.clear();
			m_mode = parse_mode::esc_param_n;
			set_busy(cycle, 2);
		} else if (b2 == 0x00 || b2 == 0x03 || b2 == 0x0f) {
			mark_decoded_command();
			try_finalize_m6();
			m_mode = parse_mode::normal;
			set_busy(cycle, 2);
		} else {
			log_unknown_escape(b2);
			m_mode = parse_mode::normal;
			set_busy(cycle, 2);
		}
		break;
	}
	case parse_mode::esc_param_n: {
		m_pending_esc_args.push_back(data);
		mark_decoded_command();
		try_finalize_m6();
		if (m_pending_esc_params > 0U)
			--m_pending_esc_params;
		if (m_pending_esc_params == 0U) {
			if (m_pending_esc_cmd == 0x4c && !m_pending_esc_args.empty())
				m_luminance = m_pending_esc_args[0];
			if (m_pending_esc_cmd == 0x54 && !m_pending_esc_args.empty())
				m_cursor_blink_speed = m_pending_esc_args[0];
			if (m_pending_esc_cmd == 0x48 && !m_pending_esc_args.empty()) {
				int const pos = std::clamp(int(m_pending_esc_args[0]), 0, (m_profile.rows * m_profile.columns) - 1);
				m_cursor_row = pos / m_profile.columns;
				m_cursor_col = pos % m_profile.columns;
			}
			if (m_pending_esc_cmd == 0x16 && !m_pending_esc_args.empty()) {
				m_blink_position_code = m_pending_esc_args[0];
				m_blink_enabled = true;
			}
			if (m_pending_esc_cmd == 0x1a && !m_pending_esc_args.empty())
				m_luminance = m_pending_esc_args[0];
			if (m_pending_esc_cmd == 0x1c && m_pending_esc_args.size() >= 6U) {
				std::uint8_t const code = m_pending_esc_args[0];
				for (int i = 0; i < 5; ++i)
					m_user_glyph[code][std::size_t(i)] = m_pending_esc_args[std::size_t(i + 1)];
				m_user_glyph_valid[code] = 1U;
				for (int r = 0; r < m_profile.rows; ++r) {
					for (int c = 0; c < m_profile.columns; ++c) {
						std::uint8_t const cell_ch = static_cast<std::uint8_t>(m_cells[std::size_t(r * m_profile.columns + c)]);
						if (cell_ch == code)
							rebuild_cell_dots(r, c);
					}
				}
			}
			m_pending_esc_cmd = 0;
			m_pending_esc_args.clear();
			m_mode = parse_mode::normal;
		}
		set_busy(cycle, 2);
		break;
	}
	}
}

std::string millennium_vfd_model::export_snapshot_json() const
{
	std::ostringstream o;
	o << "{\"variant\":\"" << json_escape(m_profile.variant) << "\",\"rows\":" << m_profile.rows
	  << ",\"columns\":" << m_profile.columns << ",\"dot_rows\":" << (m_profile.rows * 7) << ",\"dot_cols\":"
	  << (m_profile.columns * 5) << ",\"text\":[";
	for (int r = 0; r < m_profile.rows; ++r) {
		if (r)
			o << ',';
		std::string line;
		line.resize(std::size_t(m_profile.columns));
		for (int c = 0; c < m_profile.columns; ++c)
			line[std::size_t(c)] = m_cells[std::size_t(r * m_profile.columns + c)];
		o << '"' << json_escape(line) << '"';
	}
	o << "],\"dot_plane\":[";
	for (int r = 0; r < m_profile.rows * 7; ++r) {
		if (r)
			o << ',';
		o << '"';
		for (int c = 0; c < m_profile.columns * 5; ++c)
			o << (m_dot_plane[std::size_t(r * (m_profile.columns * 5) + c)] ? '1' : '0');
		o << '"';
	}
	o << "],\"raw\":\"" << json_escape(raw_hex_spaced(m_raw_log)) << "\"}\n";
	return o.str();
}

std::string millennium_vfd_model::first_text_row() const
{
	if (m_profile.rows < 1 || m_profile.columns < 1)
		return {};
	std::string line;
	line.resize(std::size_t(m_profile.columns));
	for (int c = 0; c < m_profile.columns; ++c)
		line[std::size_t(c)] = m_cells[std::size_t(c)];
	return line;
}

static bool parse_json_text_rows(std::string const &json, std::vector<std::string> &rows_out)
{
	rows_out.clear();
	auto const k = json.find("\"text\"");
	if (k == std::string::npos)
		return false;
	auto lb = json.find('[', k);
	if (lb == std::string::npos)
		return false;
	std::size_t i = lb + 1;
	while (i < json.size()) {
		while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
			++i;
		if (i >= json.size() || json[i] == ']')
			break;
		if (json[i] != '"')
			return false;
		++i;
		std::string cell;
		while (i < json.size()) {
			if (json[i] == '\\' && i + 1 < json.size()) {
				cell.push_back(json[i + 1]);
				i += 2;
				continue;
			}
			if (json[i] == '"')
				break;
			cell.push_back(json[i]);
			++i;
		}
		if (i >= json.size() || json[i] != '"')
			return false;
		++i;
		rows_out.push_back(std::move(cell));
		while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' || json[i] == '\r'))
			++i;
		if (i < json.size() && json[i] == ',')
			++i;
	}
	return !rows_out.empty();
}

bool millennium_vfd_model::text_rows_match_fixture_json(std::string const &fixture_json) const
{
	std::vector<std::string> ref;
	if (!parse_json_text_rows(fixture_json, ref))
		return false;
	if (int(ref.size()) != m_profile.rows)
		return false;
	for (int r = 0; r < m_profile.rows; ++r) {
		std::string const &line = ref[static_cast<std::size_t>(r)];
		if (int(line.size()) != m_profile.columns)
			return false;
		for (int c = 0; c < m_profile.columns; ++c) {
			if (m_cells[static_cast<std::size_t>(r * m_profile.columns + c)] != line[static_cast<std::size_t>(c)])
				return false;
		}
	}
	return true;
}
