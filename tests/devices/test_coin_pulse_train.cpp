// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"
#include "millennium_coin_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

int count_rising_pulse_edges(millennium_coin_model &m, std::uint64_t max_cycle)
{
	bool prev = false;
	int edges = 0;
	for (std::uint64_t c = 0; c < max_cycle; ++c) {
		bool const p = (m.read_status(c) & 0x01U) != 0;
		if (p && !prev)
			++edges;
		prev = p;
	}
	return edges;
}

} // namespace

int main()
{
	std::ifstream in(emu_root() / "fixtures/board/board-profile-2line-vfd.json");
	std::string const js((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	millennium_coin_board_config cfg{};
	std::string err;
	if (!millennium_board_parse_coin_profile(js, cfg, err)) {
		std::cerr << err << '\n';
		return 1;
	}
	millennium_coin_model m;
	m.configure(cfg);
	m.set_cpu_hz(12288000ULL);

	std::uint64_t const hz = 12288000ULL;
	std::uint64_t const pw = m.pulse_width_cycles(hz);
	std::uint64_t const gap = m.inter_pulse_gap_cycles(hz);

	for (int cents : cfg.denominations_cents) {
		m.reset();
		std::uint64_t const t0 = 100000ULL;
		if (!m.begin_insert_cents(cents, t0, hz)) {
			std::cerr << "begin_insert failed " << cents << '\n';
			return 1;
		}
		int const expect_pulses = cents / 5;
		std::uint64_t const n = std::uint64_t(expect_pulses);
		std::uint64_t const total = n * pw + (n > 0 ? (n - 1U) : 0U) * gap;
		std::uint64_t const maxc = t0 + total + pw * 4;
		int const edges = count_rising_pulse_edges(m, maxc);
		if (edges != expect_pulses) {
			std::cerr << "denom " << cents << " edges " << edges << " expect " << expect_pulses << '\n';
			return 1;
		}
	}
	return 0;
}
