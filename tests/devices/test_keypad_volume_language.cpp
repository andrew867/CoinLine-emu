// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

#include <iostream>

int main()
{
	millennium_keypad_model m;
	m.reset();

	struct case_ {
		int bit;
		std::uint8_t expect_high_masked;
	} const cases[] = {
		{millennium_keypad_model::k_mask_vol_up, 0x0e},
		{millennium_keypad_model::k_mask_vol_down, 0x0d},
		{millennium_keypad_model::k_mask_lang, 0x0b},
		{millennium_keypad_model::k_mask_dial_a, 0x07},
	};

	for (auto const &t : cases) {
		std::uint32_t const km = 1U << unsigned(t.bit);
		std::uint8_t const pc = m.read_port_c(km, 0);
		std::uint8_t const high = static_cast<std::uint8_t>((pc >> 4) & 0x0fU);
		if (high != t.expect_high_masked) {
			std::cerr << "vol/lang high nibble mismatch for bit " << t.bit << "\n";
			return 1;
		}
		if ((pc & 0x0fU) != 0x0fU) {
			std::cerr << "vol/lang low nibble should be idle-high\n";
			return 1;
		}
	}
	return 0;
}
