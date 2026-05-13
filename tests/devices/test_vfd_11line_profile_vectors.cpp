// SPDX-License-Identifier: GPL-2.0-or-later
// 11-line explicit semantics aligned to terminal_07_vfd_11line_emulator_spec.yaml.

#include "millennium_vfd_profile_model.h"

#include <iostream>

int main()
{
	coinline::vfd::profile_11line_model m;
	m.reset();

	m.set_active_page(2U);
	m.set_active_line(7);
	m.set_blink_enabled(true);
	m.set_softkey_label(0U, "F1");
	m.set_softkey_label(9U, "F10");

	if (m.active_page() != 2U || m.active_line() != 7 || !m.blink_enabled()) {
		std::cerr << "terminal_07 page/line/blink semantics failed\n";
		return 1;
	}
	if (m.softkey_label(0U) != "F1" || m.softkey_label(9U) != "F10") {
		std::cerr << "terminal_07 softkey labeling semantics failed\n";
		return 2;
	}

	std::cout << "vfd_11line_profile_vectors ok\n";
	return 0;
}
