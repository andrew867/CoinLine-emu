// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// terminal_21_user_io_profile_matrix.yaml — `profile_id` values (hardware harness selection).
enum class millennium_terminal21_user_io_profile : std::uint8_t {
	repdial_5,
	repdial_10,
	vfd_11line_softkeys,
};

struct millennium_keypad_board_config {
	int debounce_cycles = 6;
	int scan_min_total_reads = 16;
	int scan_min_pb_deltas = 4;
	/// Count of `quick_access_keys` entries in board JSON (0 = absent / unknown).
	unsigned quick_access_key_count = 0;
	millennium_terminal21_user_io_profile terminal_21_profile = millennium_terminal21_user_io_profile::repdial_10;
	/// When true, `terminal_21_profile` was set from `keypad.terminal_21_profile_id` and must not be inferred.
	bool terminal_21_profile_explicit = false;
};

class millennium_keypad_model {
public:
	void configure(millennium_keypad_board_config const &cfg);
	void reset();

	void write_port_b(std::uint8_t data, std::uint64_t cycle);
	void write_port_c(std::uint8_t data, std::uint64_t cycle);
	void write_control(std::uint8_t data, std::uint64_t cycle);

	std::uint8_t read_port_a(std::uint32_t keymatrix_bits, std::uint64_t cycle);
	std::uint8_t read_port_b(std::uint32_t keymatrix_bits, std::uint64_t cycle);
	std::uint8_t read_port_c(std::uint32_t keymatrix_bits, std::uint64_t cycle);

	bool consume_m7_pending() noexcept;
	std::uint64_t matrix_read_count() const noexcept { return m_total_pa_reads; }

	static int matrix_row_col_to_keymask_bit(int row, int col) noexcept;
	/// Hook debounce for the **telephony (TP) path** only — CP PIO does not sample the panel bitmap.
	std::uint32_t keymatrix_with_hook_debounce(std::uint32_t keymatrix_bits, std::uint64_t cycle);
	static constexpr int k_mask_vol_up = 16;
	static constexpr int k_mask_vol_down = 17;
	static constexpr int k_mask_lang = 18;
	static constexpr int k_mask_hook = 19;
	/// DTMF “A” (same KEYMATRIX bit as row-0 column-3 matrix position).
	static constexpr int k_mask_dial_a = 30;

private:
	millennium_keypad_board_config m_cfg{};
	std::uint8_t m_port_b_latch = 0xff;
	std::uint8_t m_port_c_latch = 0xff;

	std::uint64_t m_last_hook_raw_change_cycle = 0;
	bool m_hook_raw = false;
	bool m_hook_stable = false;

	std::uint64_t m_total_pa_reads = 0;
	std::uint8_t m_last_pb_for_scan = 0xff;
	unsigned m_pb_scan_changes = 0;
	bool m_m7_pending = false;
	bool m_m7_latched = false;

	int active_column_from_port_b(std::uint8_t pb) const noexcept;
	std::uint32_t apply_hook_debounce(std::uint32_t keymatrix_bits, std::uint64_t cycle);
	void observe_scan(std::uint8_t pb);
};
