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
	if (!m.parse_fixture_json(js, err)) {
		std::cerr << err << '\n';
		return 1;
	}
	std::uint64_t const hz = 12288000ULL;
	m.insert_card_at(0ULL, hz);
	std::uint64_t const need = 50000ULL;
	std::uint8_t const b = m.read_fifo(need);
	if (b != 0x3bU) {
		std::cerr << "first ATR byte mismatch\n";
		return 1;
	}
	return 0;
}
