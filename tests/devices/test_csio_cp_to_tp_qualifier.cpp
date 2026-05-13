// SPDX-License-Identifier: GPL-2.0-or-later
//
// Regression tests for millennium_state::qualify_cp_to_tp_csio_byte gates.
// The full driver is built inside external MAME; this file mirrors the two
// behavioral fixes and asserts the source still contains them.

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

// Mirrors millennium_state.cpp: insufficient TE-edge bookkeeping vs catalog opcodes.
[[nodiscard]] constexpr bool reject_insufficient_edges(unsigned shift_edges, bool known_single) noexcept
{
	return shift_edges < 8U && !known_single;
}

// Mirrors millennium_state.cpp: wrong_direction mirror-readback (RX active only).
[[nodiscard]] constexpr bool reject_wrong_direction(bool rx_path_active, bool in_frame_context, bool known_single,
	bool var_len_hdr) noexcept
{
	return rx_path_active && !in_frame_context && !known_single && !var_len_hdr;
}

[[nodiscard]] bool read_all(std::string const &path, std::string &out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	std::ostringstream ss;
	ss << f.rdbuf();
	out = ss.str();
	return true;
}

[[nodiscard]] bool contains_normalized(std::string_view haystack, std::string_view needle)
{
	std::string h(haystack.begin(), haystack.end());
	std::string n(needle.begin(), needle.end());
	for (auto &c : h)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	for (auto &c : n)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return h.find(n) != std::string::npos;
}

} // namespace

int main()
{
	// --- Behavioral mirrors (must stay aligned with millennium_state.cpp) ---
	if (reject_insufficient_edges(0U, false) != true) {
		std::cerr << "expected insufficient-edge rejection for non-catalog byte\n";
		return 1;
	}
	if (reject_insufficient_edges(0U, true) != false) {
		std::cerr << "catalog opcode must pass edge-count gate even when shift_edges < 8\n";
		return 2;
	}
	if (reject_insufficient_edges(8U, false) != false) {
		std::cerr << "non-catalog byte with enough edges must pass edge-count gate\n";
		return 3;
	}

	if (reject_wrong_direction(true, false, false, false) != true) {
		std::cerr << "expected wrong_direction when TP->CP RX path active and payload-like byte\n";
		return 4;
	}
	if (reject_wrong_direction(false, false, false, false) != false) {
		std::cerr << "must not wrong_direction when RX path idle\n";
		return 5;
	}
	if (reject_wrong_direction(true, true, false, false) != false) {
		std::cerr << "must not wrong_direction inside host TX frame context\n";
		return 6;
	}
	if (reject_wrong_direction(true, false, true, false) != false) {
		std::cerr << "must not wrong_direction for catalog single-byte ops during RX activity\n";
		return 7;
	}

	// --- Source contract: fixes remain in millennium_state.cpp ---
	std::string src;
	std::string const path = std::string(COINLINE_EMU_SOURCE_DIR) + "/src/mame/coinline/millennium_state.cpp";
	if (!read_all(path, src)) {
		std::cerr << "failed to read " << path << '\n';
		return 8;
	}
	if (!contains_normalized(src, "m_csio_tx_shift_edge_count < 8u && !known_single")) {
		std::cerr << "millennium_state.cpp missing insufficient_edge gate with known_single exemption\n";
		return 9;
	}
	if (!contains_normalized(src, "rx_path_active && !in_frame_context && !known_single && !var_len_hdr")) {
		std::cerr << "millennium_state.cpp missing rx-only wrong_direction gate\n";
		return 10;
	}

	std::cout << "csio_cp_to_tp_qualifier ok\n";
	return 0;
}
