// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main()
{
	std::string const path = std::string(COINLINE_EMU_SOURCE_DIR) + "/fixtures/modem/clean-connect.hex";
	std::ifstream in(path);
	if (!in) {
		std::cerr << "missing clean-connect.hex\n";
		return 1;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	std::string err;
	millennium_modem_model m;
	m.reset();
	if (!m.replay_fixture_text(ss.str(), err)) {
		std::cerr << err << "\n";
		return 1;
	}
	if (m.state() != millennium_modem_state::connected) {
		std::cerr << "expected connected after clean-connect replay\n";
		return 1;
	}
	return 0;
}
