// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

#include <iostream>

namespace {

void scan_triplet(millennium_keypad_model &m, std::uint32_t km, std::uint64_t &cy)
{
	for (std::uint8_t pb :
		{std::uint8_t(0xfe), std::uint8_t(0xfd), std::uint8_t(0xfb), std::uint8_t(0xf7)}) {
		m.write_port_b(pb, cy);
		(void)m.read_port_a(km, cy++);
	}
}

} // namespace

int main()
{
	millennium_keypad_board_config cfg{};
	cfg.debounce_cycles = 0;
	cfg.scan_min_total_reads = 4;
	cfg.scan_min_pb_deltas = 2;

	millennium_keypad_model m;
	m.configure(cfg);
	m.reset();

	std::uint64_t cy = 0;
	std::uint32_t const km = 0;
	for (int i = 0; i < 20; ++i)
		scan_triplet(m, km, cy);

	if (!m.consume_m7_pending()) {
		std::cerr << "expected M7 pending after repeated column scans\n";
		return 1;
	}
	if (m.matrix_read_count() < 4ULL) {
		std::cerr << "expected matrix_read_count >= configured minimum\n";
		return 1;
	}
	return 0;
}
