// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <iostream>

int main()
{
	for (int run = 0; run < 2; ++run) {
		millennium_modem_model m;
		m.reset();
		m.note_asci_programmed(0x88, 0x99);
		if (!m.consume_m8_pending()) {
			std::cerr << "M8 not pending run " << run << "\n";
			return 1;
		}
		if (m.consume_m8_pending()) {
			std::cerr << "M8 should be one-shot run " << run << "\n";
			return 1;
		}
	}
	return 0;
}
