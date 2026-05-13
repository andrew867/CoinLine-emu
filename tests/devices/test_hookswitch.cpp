// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

#include <iostream>

namespace {

std::uint8_t read_matrix_key1(millennium_keypad_model &m, std::uint32_t km, std::uint64_t cy)
{
	m.write_port_b(0xfe, cy);
	return m.read_port_a(km, cy);
}

} // namespace

int main()
{
	millennium_keypad_board_config cfg{};
	cfg.debounce_cycles = 6;
	cfg.scan_min_total_reads = 10000;
	cfg.scan_min_pb_deltas = 10000;

	millennium_keypad_model m;
	m.configure(cfg);
	m.reset();

	std::uint32_t const km_key = 1U << 0;
	for (std::uint64_t cy = 0; cy < 40; ++cy) {
		bool const hook_raw = (cy % 3U) == 0U;
		std::uint32_t const km = km_key | (hook_raw ? (1U << millennium_keypad_model::k_mask_hook) : 0U);
		std::uint8_t const pa = read_matrix_key1(m, km, cy);
		if (pa != 0x0e) {
			std::cerr << "matrix read corrupted by hookswitch chatter at cy " << cy << "\n";
			return 1;
		}
	}
	return 0;
}
