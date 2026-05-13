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
	std::string const txt = read_all("fixtures/modem/noisy-line.hex");
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
	if (m.state() != millennium_modem_state::noisy_line) {
		std::cerr << "expected noisy_line\n";
		return 1;
	}
	auto const &t = m.transcript();
	if (t.empty()) {
		std::cerr << "expected transcript\n";
		return 1;
	}
	bool saw_noise = false;
	for (auto const &e : t) {
		if (e.direction == 'r' && (e.byte == 0xff || e.byte == 0xee))
			saw_noise = true;
	}
	if (!saw_noise) {
		std::cerr << "expected corrupted rx bytes in transcript\n";
		return 1;
	}
	return 0;
}
