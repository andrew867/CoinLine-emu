// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_security_model.h"

#include <iostream>

int main()
{
	millennium_security_board_config cfg{};
	cfg.debounce_cycles = 3;

	millennium_security_model s;
	s.configure(cfg);
	s.reset();

	for (unsigned line = 0; line < 4; ++line) {
		std::uint32_t const bits = 1U << line;
		if (s.read_lines(bits, 0) != 0) {
			std::cerr << "line " << line << " should not assert before debounce\n";
			return 1;
		}
		if (s.read_lines(bits, 3) != static_cast<std::uint8_t>(1U << line)) {
			std::cerr << "line " << line << " should assert after debounce\n";
			return 1;
		}
		s.reset();
	}

	s.reset();
	if (s.read_lines(0x0f, 0) != 0) {
		std::cerr << "all-asserted raw should not appear before debounce settles\n";
		return 1;
	}
	if (s.read_lines(0x0f, 3) != 0x0f) {
		std::cerr << "all-high raw should read as all asserted after debounce\n";
		return 1;
	}
	return 0;
}
