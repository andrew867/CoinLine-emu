// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_card_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	{
		std::ifstream in(emu_root() / "fixtures/cards/magcard-valid.json");
		std::string const js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		millennium_card_model m;
		std::string err;
		if (!m.parse_fixture_json(js, err) || !m.lrc_ok()) {
			std::cerr << "valid fixture should pass LRC\n";
			return 1;
		}
	}
	{
		std::ifstream in(emu_root() / "fixtures/cards/magcard-invalid-lrc.json");
		std::string const js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		millennium_card_model m;
		std::string err;
		if (!m.parse_fixture_json(js, err) || m.lrc_ok()) {
			std::cerr << "invalid LRC fixture must fail validation\n";
			return 1;
		}
	}
	return 0;
}
