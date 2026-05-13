// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <iostream>

int main()
{
	millennium_modem_model m;
	m.reset();
	m.note_tx('H');
	m.note_tx('i');
	m.push_rx('o');
	m.push_rx('k');

	auto const &t = m.transcript();
	if (t.size() != 4) {
		std::cerr << "transcript length\n";
		return 1;
	}
	if (t[0].direction != 't' || t[0].byte != 'H')
		return 1;
	if (t[1].direction != 't' || t[1].byte != 'i')
		return 1;
	if (t[2].direction != 'r' || t[2].byte != 'o')
		return 1;
	if (t[3].direction != 'r' || t[3].byte != 'k')
		return 1;
	return 0;
}
