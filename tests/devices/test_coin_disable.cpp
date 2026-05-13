// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_coin_model.h"

#include <iostream>

int main()
{
	millennium_coin_board_config cfg{};
	cfg.denominations_cents = {25};
	millennium_coin_model m;
	m.configure(cfg);
	m.set_cpu_hz(12288000ULL);
	if (!m.begin_insert_cents(25, 0ULL, 12288000ULL)) {
		std::cerr << "expected insert ok\n";
		return 1;
	}
	m.reset();
	m.write_control(0x01, 0);
	if (m.begin_insert_cents(25, 0ULL, 12288000ULL)) {
		std::cerr << "insert should fail when disabled\n";
		return 1;
	}
	if ((m.read_status(0) & 0x10U) == 0) {
		std::cerr << "disabled bit\n";
		return 1;
	}
	return 0;
}
