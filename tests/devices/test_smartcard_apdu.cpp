// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_smartcard_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/cards/smartcard-valid.json");
	std::string const js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	millennium_smartcard_model m;
	std::string err;
	if (!m.parse_fixture_json(js, err))
		return 1;
	if (m.protocol() == "memory") {
		// Microprocessor APDU exchange is exercised when protocol is t0/t1 and fixtures supply apdu_responses (future).
		return 0;
	}
	return 0;
}
