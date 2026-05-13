// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

namespace {

// 4×4 telephone matrix: (row,col) → KEYMATRIX bit index (see millennium.cpp PORT_BIT order).
// Column 3 is the DTMF letter column (A–D map to dedicated KEYMATRIX bits per bezel wiring).
constexpr int k_bit[4][4] = {
	{ 0, 1, 2, 30 }, // 1 2 3 A  (A → k_mask_dial_a)
	{ 3, 4, 5, 13 }, // 4 5 6 B
	{ 6, 7, 8, 14 }, // 7 8 9 C
	{ 9, 10, 11, 15 }, // * 0 # D
};

} // namespace

void millennium_keypad_model::configure(millennium_keypad_board_config const &cfg)
{
	m_cfg = cfg;
	if (m_cfg.debounce_cycles < 0)
		m_cfg.debounce_cycles = 0;
	if (m_cfg.scan_min_total_reads < 1)
		m_cfg.scan_min_total_reads = 1;
	if (m_cfg.scan_min_pb_deltas < 1)
		m_cfg.scan_min_pb_deltas = 1;
}

void millennium_keypad_model::reset()
{
	m_port_b_latch = 0xff;
	m_port_c_latch = 0xff;
	m_last_hook_raw_change_cycle = 0;
	m_hook_raw = false;
	m_hook_stable = false;
	m_total_pa_reads = 0;
	m_last_pb_for_scan = 0xff;
	m_pb_scan_changes = 0;
	m_m7_pending = false;
	m_m7_latched = false;
}

void millennium_keypad_model::write_port_b(std::uint8_t data, std::uint64_t cycle)
{
	(void)cycle;
	m_port_b_latch = data;
}

void millennium_keypad_model::write_port_c(std::uint8_t data, std::uint64_t cycle)
{
	(void)cycle;
	m_port_c_latch = data;
}

void millennium_keypad_model::write_control(std::uint8_t data, std::uint64_t cycle)
{
	(void)data;
	(void)cycle;
}

int millennium_keypad_model::matrix_row_col_to_keymask_bit(int row, int col) noexcept
{
	if (row < 0 || row > 3 || col < 0 || col > 3)
		return -1;
	return k_bit[row][col];
}

int millennium_keypad_model::active_column_from_port_b(std::uint8_t pb) const noexcept
{
	unsigned const active_low_high_cols = (~unsigned(pb) >> 4) & 0xfU;
	if (active_low_high_cols != 0) {
		for (int i = 0; i < 4; ++i) {
			if (active_low_high_cols & (1U << i))
				return i;
		}
	}

	unsigned const active_low_cols = (~unsigned(pb)) & 0xfU;
	if (active_low_cols == 0)
		return -1;
	for (int i = 0; i < 4; ++i) {
		if (active_low_cols & (1U << i))
			return i;
	}
	return -1;
}

std::uint32_t millennium_keypad_model::apply_hook_debounce(std::uint32_t keymatrix_bits, std::uint64_t cycle)
{
	bool const raw = (keymatrix_bits & (1U << k_mask_hook)) != 0;
	if (raw != m_hook_raw) {
		m_hook_raw = raw;
		m_last_hook_raw_change_cycle = cycle;
	}
	if (m_cfg.debounce_cycles <= 0
		|| cycle >= m_last_hook_raw_change_cycle + std::uint64_t(m_cfg.debounce_cycles))
		m_hook_stable = m_hook_raw;

	std::uint32_t out = keymatrix_bits & ~(1U << k_mask_hook);
	if (m_hook_stable)
		out |= (1U << k_mask_hook);
	return out;
}

std::uint32_t millennium_keypad_model::keymatrix_with_hook_debounce(std::uint32_t keymatrix_bits, std::uint64_t cycle)
{
	return apply_hook_debounce(keymatrix_bits, cycle);
}

void millennium_keypad_model::observe_scan(std::uint8_t pb)
{
	++m_total_pa_reads;
	if (pb != m_last_pb_for_scan) {
		m_last_pb_for_scan = pb;
		++m_pb_scan_changes;
	}
	if (!m_m7_latched && m_total_pa_reads >= std::uint64_t(m_cfg.scan_min_total_reads)
		&& m_pb_scan_changes >= unsigned(m_cfg.scan_min_pb_deltas)) {
		m_m7_pending = true;
		m_m7_latched = true;
	}
}

bool millennium_keypad_model::consume_m7_pending() noexcept
{
	bool const v = m_m7_pending;
	m_m7_pending = false;
	return v;
}

std::uint8_t millennium_keypad_model::read_port_a(std::uint32_t keymatrix_bits, std::uint64_t cycle)
{
	// CP PIO does not see the front panel; this path is for unit tests and column-strobe
	// heuristics. Hook smoothing is only applied in `keymatrix_with_hook_debounce` (TP path).
	std::uint32_t const keys = keymatrix_bits;
	observe_scan(m_port_b_latch);

	// Board scan: PB strobes one column (active-low), PA returns row lines (active-low).
	int const active_col = active_column_from_port_b(m_port_b_latch);
	std::uint8_t rows = 0x0f;
	if (active_col >= 0 && active_col <= 3) {
		for (int row = 0; row < 4; ++row) {
			unsigned const bit = unsigned(k_bit[row][active_col]);
			if ((keys & (1U << bit)) != 0)
				rows &= static_cast<std::uint8_t>(~(1U << row));
		}
	} else {
		for (int col = 0; col < 4; ++col) {
			for (int row = 0; row < 4; ++row) {
				unsigned const bit = unsigned(k_bit[row][col]);
				if ((keys & (1U << bit)) != 0)
					rows &= static_cast<std::uint8_t>(~(1U << row));
			}
		}
	}
	return rows & 0x0f;
}

std::uint8_t millennium_keypad_model::read_port_b(std::uint32_t keymatrix_bits, std::uint64_t cycle)
{
	(void)keymatrix_bits;
	(void)cycle;
	return m_port_b_latch;
}

std::uint8_t millennium_keypad_model::read_port_c(std::uint32_t keymatrix_bits, std::uint64_t cycle)
{
	(void)cycle;
	std::uint32_t const keys = keymatrix_bits;
	// Low nibble: New Call + DTMF B/C/D (KEYMATRIX bits 12–15), active-low when pressed.
	std::uint8_t low = 0x0f;
	for (int i = 0; i < 4; ++i) {
		if ((keys & (1U << (12 + i))) != 0)
			low &= static_cast<std::uint8_t>(~(1U << i));
	}
	// High nibble: volume, language, dial-pad A; active-low when pressed.
	std::uint8_t high = 0x0f;
	if ((keys & (1U << 16)) != 0)
		high &= static_cast<std::uint8_t>(~1U);
	if ((keys & (1U << 17)) != 0)
		high &= static_cast<std::uint8_t>(~2U);
	if ((keys & (1U << 18)) != 0)
		high &= static_cast<std::uint8_t>(~4U);
	if ((keys & (1U << k_mask_dial_a)) != 0)
		high &= static_cast<std::uint8_t>(~8U);
	return static_cast<std::uint8_t>((high << 4) | (low & 0x0f));
}
