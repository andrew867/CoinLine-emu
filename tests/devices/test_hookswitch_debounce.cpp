// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

#include <iostream>

namespace {

std::uint8_t read_key4_col0(millennium_keypad_model &m, std::uint32_t km, std::uint64_t cy)
{
	m.write_port_b(0xfe, cy);
	return m.read_port_a(km, cy);
}

} // namespace

int main()
{
	millennium_keypad_board_config cfg{};
	cfg.debounce_cycles = 5;
	cfg.scan_min_total_reads = 10000;
	cfg.scan_min_pb_deltas = 10000;

	millennium_keypad_model m;
	m.configure(cfg);
	m.reset();

	std::uint32_t const km_key = 1U << 3;
	std::uint32_t const km_hook = 1U << millennium_keypad_model::k_mask_hook;

	for (std::uint64_t cy = 0; cy < 4; ++cy) {
		std::uint8_t const pa = read_key4_col0(m, km_key | km_hook, cy);
		if (pa != 0x0d) {
			std::cerr << "expected row-1-low for key 4 before hook debounce settles\n";
			return 1;
		}
	}
	for (std::uint64_t cy = 4; cy < 30; ++cy) {
		std::uint8_t const pa = read_key4_col0(m, km_key | km_hook, cy);
		if (pa != 0x0d) {
			std::cerr << "matrix read changed after hook debounce window\n";
			return 1;
		}
	}
	m.reset();
	for (std::uint64_t cy = 0; cy < 4; ++cy) {
		std::uint8_t const pa = read_key4_col0(m, km_key | km_hook, cy);
		if (pa != 0x0d) {
			std::cerr << "after reset matrix should ignore unsettled hook for row/col path\n";
			return 1;
		}
	}
	return 0;
}
