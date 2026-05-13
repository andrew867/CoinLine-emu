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
	m.inject_jam(0);
	if ((m.read_status(1000) & 0x04U) == 0) {
		std::cerr << "jam bit not set\n";
		return 1;
	}
	m.write_control(0x02, 1001);
	if ((m.read_status(2000) & 0x04U) != 0) {
		std::cerr << "jam should clear after ack write\n";
		return 1;
	}
	return 0;
}
