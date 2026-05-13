// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

struct millennium_security_board_config {
	int debounce_cycles = 8;
};

class millennium_security_model {
public:
	void configure(millennium_security_board_config const &cfg);
	void reset();

	std::uint8_t read_lines(std::uint32_t sec_ioport_bits, std::uint64_t cycle);

private:
	millennium_security_board_config m_cfg{};
	std::uint8_t m_stable = 0;
	std::uint8_t m_last_raw = 0;
	std::uint64_t m_last_change_cycle = 0;
};
