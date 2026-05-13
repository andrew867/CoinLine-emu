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
	std::uint64_t const hz = 12288000ULL;
	std::uint64_t const t = 100000ULL;
	m.insert_card_at(0ULL, hz);
	for (int i = 0; i < 8; ++i)
		(void)m.read_fifo(t);
	m.write_command(0xb0, t);
	m.write_command(0x05, t);
	std::uint8_t const v = m.read_fifo(t + 1000ULL);
	if (m.memory().size() <= 5) {
		std::cerr << "fixture memory too small\n";
		return 1;
	}
	if (v != m.memory()[5]) {
		std::cerr << "memory read mismatch\n";
		return 1;
	}
	return 0;
}
