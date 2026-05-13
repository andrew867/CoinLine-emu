// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

#include <iostream>

int main()
{
	millennium_keypad_model m;
	m.reset();

	for (int i = 0; i < 4; ++i) {
		std::uint32_t const km = 1U << unsigned(12 + i);
		std::uint8_t const pc = m.read_port_c(km, 0);
		std::uint8_t const low = static_cast<std::uint8_t>(pc & 0x0fU);
		std::uint8_t const expect_low = static_cast<std::uint8_t>(0x0fU & ~(1U << i));
		if (low != expect_low) {
			std::cerr << "quick key " << i << " low nibble mismatch\n";
			return 1;
		}
		if ((pc & 0xf0U) != 0xf0U) {
			std::cerr << "quick key " << i << " high nibble should be idle-high\n";
			return 1;
		}
	}
	return 0;
}
