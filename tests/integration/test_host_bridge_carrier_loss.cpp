// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_all(char const *rel)
{
	std::ifstream in(std::string(COINLINE_EMU_SOURCE_DIR) + "/" + rel);
	if (!in)
		return {};
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

} // namespace

int main()
{
	std::string const txt = read_all("fixtures/modem/dropped-carrier.hex");
	if (txt.empty()) {
		std::cerr << "missing fixture\n";
		return 1;
	}
	std::string err;
	millennium_modem_model m;
	m.reset();
	if (!m.replay_fixture_text(txt, err)) {
		std::cerr << err << "\n";
		return 1;
	}
	if (m.state() != millennium_modem_state::carrier_lost) {
		std::cerr << "expected carrier_lost\n";
		return 1;
	}
	if (!m.inject_event("recover_idle")) {
		std::cerr << "recover failed\n";
		return 1;
	}
	if (m.state() != millennium_modem_state::idle) {
		std::cerr << "expected idle after recovery\n";
		return 1;
	}
	return 0;
}
