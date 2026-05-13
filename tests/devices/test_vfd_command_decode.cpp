// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_vfd_model.h"

#include <iostream>

int main()
{
	millennium_display_profile prof{};
	prof.rows = 2;
	prof.columns = 20;
	prof.variant = "2line";
	prof.busy_cycles_char = 10;
	prof.busy_cycles_clear = 50;
	prof.busy_cycles_cursor = 8;

	millennium_vfd_model v;
	v.configure(prof);
	v.reset();

	v.write(0x41, 0);
	if (v.status_read(0) != 0x80) {
		std::cerr << "expected busy after char write\n";
		return 1;
	}
	if (v.status_read(9) != 0x80) {
		std::cerr << "expected still busy before threshold\n";
		return 1;
	}
	if (v.status_read(10) != 0x00) {
		std::cerr << "expected ready after busy_cycles_char\n";
		return 1;
	}

	v.write(0x0c, 100);
	if (v.status_read(100) != 0x80) {
		std::cerr << "expected busy after clear\n";
		return 1;
	}
	if (v.status_read(149) != 0x80) {
		std::cerr << "expected busy through clear window\n";
		return 1;
	}
	if (v.status_read(150) != 0x00) {
		std::cerr << "expected ready after clear busy window\n";
		return 1;
	}

	v.reset();
	v.write(0x1b, 0);
	v.write(0x40, 1);
	if (v.status_read(1000) != 0x00) {
		std::cerr << "expected ready long after ESC @\n";
		return 1;
	}

	v.reset();
	v.write(0x1b, 0);
	v.write(0x48, 1);
	v.write(1, 2);
	v.write(3, 3);
	if (v.first_text_row().size() != 20U) {
		std::cerr << "unexpected row width\n";
		return 1;
	}

	return 0;
}
