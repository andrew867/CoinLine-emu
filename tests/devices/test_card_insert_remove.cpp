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
	std::ifstream in(emu_root() / "fixtures/cards/magcard-valid.json");
	std::string const js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	millennium_card_model m;
	std::string err;
	if (!m.parse_fixture_json(js, err))
		return 1;
	m.arm_swipe(5000ULL, 12288000ULL);
	if ((m.status_bits(6000ULL) & 4U) == 0 && m.bit_count() > 0) {
		std::cerr << "expected reading bit during swipe\n";
		return 1;
	}
	m.abort_swipe();
	if ((m.status_bits(6000ULL) & 4U) != 0) {
		std::cerr << "abort should stop reading state\n";
		return 1;
	}
	return 0;
}
