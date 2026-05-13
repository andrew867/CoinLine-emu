// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_security_model.h"

#include <iostream>

int main()
{
	millennium_security_board_config cfg{};
	cfg.debounce_cycles = 8;
	millennium_security_model s;
	s.configure(cfg);
	s.reset();

	if (s.read_lines(0x08U, 0ULL) != 0) {
		std::cerr << "service line should not assert before debounce\n";
		return 1;
	}
	if (s.read_lines(0x08U, 8ULL) != 0x08U) {
		std::cerr << "service line should assert after debounce\n";
		return 1;
	}

	s.reset();
	if (s.read_lines(0x08U, 0ULL) != 0) {
		std::cerr << "service line should not assert before debounce (post-reset)\n";
		return 1;
	}
	if (s.read_lines(0x08U, 16ULL) != 0x08U) {
		std::cerr << "service-only assertion after debounce window\n";
		return 1;
	}
	return 0;
}
