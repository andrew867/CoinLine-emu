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
	if (!m.parse_fixture_json(js, err)) {
		std::cerr << "parse: " << err << '\n';
		return 1;
	}
	std::uint64_t const cpu_hz = 12288000ULL;
	std::uint64_t const cpb = m.cycles_per_bit(cpu_hz);
	std::uint64_t const expected = cpu_hz / 210ULL;
	if (cpb != expected) {
		std::cerr << "cycles_per_bit mismatch\n";
		return 1;
	}
	m.arm_swipe(1000ULL, cpu_hz);
	std::uint64_t const base = 1000ULL;
	std::uint64_t const bit17_a = base + 17 * cpb;
	std::uint64_t const bit17_b = base + 17 * cpb + cpb;
	if ((bit17_b - bit17_a) != cpb) {
		std::cerr << "spacing\n";
		return 1;
	}
	return 0;
}
