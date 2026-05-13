// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>

struct millennium_display_profile {
	std::string variant = "2line";
	int rows = 2;
	int columns = 20;
	int busy_cycles_char = 48;
	int busy_cycles_clear = 240;
	int busy_cycles_cursor = 64;
	/// Reference snapshot JSON (fixtures/display/vfd-*-idle.json) used for M10 idle detection.
	std::string idle_fixture_relpath;
};

class millennium_vfd_model {
public:
	void configure(millennium_display_profile const &profile);
	void reset();

	/// PIO port B image: **CS** = bit 6 (`VFD_CSB`, active low → chip selected when bit clear). **A0** = bit 5 (`VFDA0`, high = command/cursor register).
	/// Default `pio_port_b = 0` means VFD selected, data register (unit-test / legacy).
	void write(std::uint8_t data, std::uint64_t cycle);
	void write(std::uint8_t data, std::uint64_t cycle, std::uint8_t pio_port_b);
	std::uint8_t status_read(std::uint64_t cycle) const;

	bool milestone_m6_met() const noexcept { return m_m6_met; }

	/// Compare visible `text[]` rows from `fixture_json` (idle/service snapshot) to the live buffer.
	bool text_rows_match_fixture_json(std::string const &fixture_json) const;

	std::string export_snapshot_json() const;
	std::string first_text_row() const;

	bool unknown_escape_pending() const noexcept { return m_last_unknown_escape; }

	std::vector<char> const &cells() const noexcept { return m_cells; }
	millennium_display_profile const &display_profile() const noexcept { return m_profile; }
	std::size_t firmware_write_bytes() const noexcept { return m_raw_log.size(); }
	std::vector<std::uint8_t> const &raw_log() const noexcept { return m_raw_log; }
	int cursor_row() const noexcept { return m_cursor_row; }
	int cursor_col() const noexcept { return m_cursor_col; }
	bool increment_write_mode() const noexcept { return m_increment_write_mode; }
	bool vertical_scroll_mode() const noexcept { return m_vertical_scroll_mode; }
	bool horizontal_scroll_mode() const noexcept { return m_horizontal_scroll_mode; }
	std::uint8_t blink_position_code() const noexcept { return m_blink_position_code; }
	bool cursor_visible() const noexcept { return m_cursor_visible; }
	int active_line() const noexcept { return m_active_line; }
	std::uint8_t active_page() const noexcept { return m_active_page; }
	bool blink_enabled() const noexcept { return m_blink_enabled; }
	std::string const &softkey_label(std::size_t idx) const noexcept { return m_softkey_labels[idx]; }
	std::uint8_t luminance() const noexcept { return m_luminance; }
	bool katakana_font() const noexcept { return m_katakana_font; }
	bool flickerless_write() const noexcept { return m_flickerless_write; }
	std::vector<std::uint8_t> const &dot_plane() const noexcept { return m_dot_plane; }
	int dot_rows() const noexcept { return m_profile.rows * 7; }
	int dot_cols() const noexcept { return m_profile.columns * 5; }

	void set_active_line(int line) noexcept;
	void set_active_page(std::uint8_t page) noexcept;
	void set_blink_enabled(bool enabled) noexcept;
	void set_softkey_label(std::size_t idx, std::string const &label);

private:
	enum class parse_mode : std::uint8_t { normal, esc, esc_param_n };

	millennium_display_profile m_profile{};
	std::vector<char> m_cells;
	std::vector<std::uint8_t> m_raw_log;
	std::vector<std::uint8_t> m_dot_plane;
	std::vector<std::uint8_t> m_decimal_point;
	std::vector<std::uint8_t> m_comma_tail;
	std::vector<std::uint8_t> m_arrow_mark;
	std::array<std::array<std::uint8_t, 5>, 256> m_user_glyph{};
	std::array<std::uint8_t, 256> m_user_glyph_valid{};
	int m_cursor_row = 0;
	int m_cursor_col = 0;
	std::uint64_t m_busy_until_cycle = 0;

	parse_mode m_mode = parse_mode::normal;
	bool m_saw_decoded_command = false;
	bool m_saw_printable_data = false;
	bool m_m6_met = false;
	bool m_last_unknown_escape = false;
	bool m_increment_write_mode = true;
	bool m_vertical_scroll_mode = false;
	bool m_horizontal_scroll_mode = false;
	bool m_cursor_visible = false;
	int m_active_line = 0;
	std::uint8_t m_active_page = 0U;
	bool m_blink_enabled = false;
	std::uint8_t m_blink_position_code = 0U;
	std::array<std::string, 10> m_softkey_labels{};
	bool m_katakana_font = false;
	bool m_flickerless_write = false;
	std::uint8_t m_luminance = 0xffU;
	std::uint8_t m_cursor_blink_speed = 0U;
	std::uint8_t m_extended_control_mode = 0U;
	std::uint8_t m_pending_esc_cmd = 0;
	std::uint8_t m_pending_esc_params = 0;
	std::vector<std::uint8_t> m_pending_esc_args;

	int cell_index() const noexcept;
	void set_busy(std::uint64_t cycle, int kind);
	void clear_display();
	void home_cursor();
	void carriage_return();
	void line_feed();
	void backspace();
	void horizontal_tab();
	void software_reset();
	void write_glyph(char ch);
	void write_glyph_byte(std::uint8_t b);
	void rebuild_cell_dots(int row, int col);
	void clear_all_dots();
	void set_dot(int row, int col, bool on);
	void mark_decoded_command();
	void try_finalize_m6();
	void append_raw(std::uint8_t b);
	void log_unknown_control(std::uint8_t b);
	void log_unknown_escape(std::uint8_t b2);
	void write_command_byte(std::uint8_t cmd, std::uint64_t cycle);
	void write_data_byte(std::uint8_t data, std::uint64_t cycle);
};
