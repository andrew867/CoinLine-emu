// SPDX-License-Identifier: GPL-2.0-or-later
//
// Replays fixtures/scenarios/keypad-smoke.json against millennium_keypad_model using the
// board profile declared in the scenario. This validates JSON shape, key masks, and that
// the scan heuristic can reach M7 during the scripted column activity.
//
// This test does not assert firmware-visible UI acknowledgment (VFD / host evidence); that
// belongs to the full MAME + scenario-runner acceptance path.

#include "millennium_board_profile.h"
#include "millennium_keypad_model.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

char const *emu_root_cstr() noexcept
{
	return COINLINE_EMU_SOURCE_DIR;
}

std::string join_path(std::string const &base, std::string const &rel)
{
	if (base.empty())
		return rel;
	char const c = base.back();
	if (c == '/' || c == '\\')
		return base + rel;
	return base + '/' + rel;
}

std::string read_text_file(std::string const &path, std::string &err)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		err = "open failed: " + path;
		return {};
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::optional<std::string> json_quoted_field(std::string const &line, std::string_view field)
{
	std::string const key = std::string("\"") + std::string(field) + "\"";
	auto const p = line.find(key);
	if (p == std::string::npos)
		return std::nullopt;
	auto const colon = line.find(':', p);
	if (colon == std::string::npos)
		return std::nullopt;
	auto q1 = line.find('"', colon + 1);
	if (q1 == std::string::npos)
		return std::nullopt;
	auto q2 = line.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return std::nullopt;
	return line.substr(q1 + 1, q2 - q1 - 1);
}

int json_int_field(std::string const &line, std::string_view field, int default_value)
{
	std::string const key = std::string("\"") + std::string(field) + "\"";
	auto const p = line.find(key);
	if (p == std::string::npos)
		return default_value;
	auto const colon = line.find(':', p);
	if (colon == std::string::npos)
		return default_value;
	std::size_t i = colon + 1;
	while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
		++i;
	int v = 0;
	bool any = false;
	while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
		any = true;
		v = v * 10 + int(line[i] - '0');
		++i;
	}
	return any ? v : default_value;
}

std::uint32_t smoke_key_mask(std::string const &key)
{
	if (key == "1")
		return 1U << 0;
	if (key == "2")
		return 1U << 1;
	if (key == "3")
		return 1U << 2;
	if (key == "4")
		return 1U << 3;
	if (key == "5")
		return 1U << 4;
	if (key == "6")
		return 1U << 5;
	if (key == "7")
		return 1U << 6;
	if (key == "8")
		return 1U << 7;
	if (key == "9")
		return 1U << 8;
	if (key == "*")
		return 1U << 9;
	if (key == "0")
		return 1U << 10;
	if (key == "#")
		return 1U << 11;
	if (key == "A")
		return 1U << unsigned(millennium_keypad_model::k_mask_dial_a);
	if (key == "B")
		return 1U << 13;
	if (key == "C")
		return 1U << 14;
	if (key == "D")
		return 1U << 15;
	if (key == "VOL+")
		return 1U << 16;
	if (key == "VOL-")
		return 1U << 17;
	if (key == "LANG")
		return 1U << 18;
	return 0;
}

void scan_columns_once(millennium_keypad_model &m, std::uint32_t km, std::uint64_t &cy, bool &saw_m7)
{
	for (std::uint8_t pb : {std::uint8_t(0xfe), std::uint8_t(0xfd), std::uint8_t(0xfb), std::uint8_t(0xf7)}) {
		m.write_port_b(pb, cy);
		(void)m.read_port_a(km, cy++);
		if (m.consume_m7_pending())
			saw_m7 = true;
	}
}

void press_key_model(millennium_keypad_model &m, std::uint32_t &km, std::uint64_t &cy, std::uint32_t mask,
	int duration_cycles, bool &saw_m7)
{
	km |= mask;
	for (int i = 0; i < duration_cycles; ++i)
		scan_columns_once(m, km, cy, saw_m7);
	km &= ~mask;
	for (int i = 0; i < 4; ++i)
		scan_columns_once(m, km, cy, saw_m7);
}

} // namespace

int main()
{
	std::string err;
	std::string const scenario_path = join_path(emu_root_cstr(), "fixtures/scenarios/keypad-smoke.json");
	std::string const scenario_txt = read_text_file(scenario_path, err);
	if (scenario_txt.empty()) {
		std::cerr << err << "\n";
		return 1;
	}

	auto const sid = json_quoted_field(scenario_txt, "scenario_id");
	if (!sid || *sid != "keypad-smoke") {
		std::cerr << "unexpected scenario_id\n";
		return 1;
	}

	auto const board_rel = json_quoted_field(scenario_txt, "board_profile");
	if (!board_rel) {
		std::cerr << "missing board_profile\n";
		return 1;
	}
	std::string const board_path = join_path(emu_root_cstr(), *board_rel);
	std::string const board_txt = read_text_file(board_path, err);
	if (board_txt.empty()) {
		std::cerr << err << "\n";
		return 1;
	}

	millennium_keypad_board_config kcfg{};
	millennium_board_parse_keypad_profile(board_txt, kcfg, err);

	unsigned expected_press_keys = 0;
	for (std::size_t z = 0; z < scenario_txt.size();) {
		std::size_t const p = scenario_txt.find("\"press_key\"", z);
		if (p == std::string::npos)
			break;
		++expected_press_keys;
		z = p + 1;
	}
	if (expected_press_keys == 0U) {
		std::cerr << "scenario contains no press_key steps\n";
		return 1;
	}

	millennium_keypad_model m;
	m.configure(kcfg);
	m.reset();

	std::uint64_t cy = 0;
	std::uint32_t km = 0;
	bool saw_m7 = false;
	unsigned press_count = 0;

	std::size_t scan = 0;
	while ((scan = scenario_txt.find("\"verb\"", scan)) != std::string::npos) {
		std::size_t const block_open = scenario_txt.rfind('{', scan);
		std::size_t const block_close = scenario_txt.find('}', scan);
		if (block_open == std::string::npos || block_close == std::string::npos || block_close <= block_open) {
			std::cerr << "malformed scenario step near offset " << scan << "\n";
			return 1;
		}
		std::string const block = scenario_txt.substr(block_open, block_close - block_open + 1);
		scan = block_close + 1;

		auto const verb = json_quoted_field(block, "verb");
		if (!verb)
			continue;
		if (*verb == "reset") {
			m.reset();
			cy = 0;
			km = 0;
			continue;
		}
		if (*verb == "run_cycles") {
			int const n = json_int_field(block, "cycles", 0);
			if (n < 0) {
				std::cerr << "invalid cycles\n";
				return 1;
			}
			cy += std::uint64_t(n);
			continue;
		}
		if (*verb == "press_key") {
			auto const key = json_quoted_field(block, "key");
			if (!key) {
				std::cerr << "press_key missing key\n";
				return 1;
			}
			int const dur = json_int_field(block, "duration_cycles", 48);
			if (dur < 1) {
				std::cerr << "invalid duration_cycles\n";
				return 1;
			}
			std::uint32_t const mask = smoke_key_mask(*key);
			if (mask == 0U) {
				std::cerr << "unknown key in scenario: " << *key << "\n";
				return 1;
			}
			press_key_model(m, km, cy, mask, dur, saw_m7);
			++press_count;
			continue;
		}
		if (*verb == "lift_handset") {
			km |= 1U << unsigned(millennium_keypad_model::k_mask_hook);
			for (int i = 0; i < 200; ++i)
				scan_columns_once(m, km, cy, saw_m7);
			continue;
		}
		if (*verb == "hang_up") {
			km &= ~(1U << unsigned(millennium_keypad_model::k_mask_hook));
			for (int i = 0; i < 200; ++i)
				scan_columns_once(m, km, cy, saw_m7);
			continue;
		}
	}

	if (press_count != expected_press_keys) {
		std::cerr << "expected " << expected_press_keys << " press_key steps, got " << press_count << "\n";
		return 1;
	}
	if (!saw_m7) {
		std::cerr << "M7 (keypad scan observed) did not trigger during scenario replay (press_count=" << press_count
			  << " matrix_reads=" << m.matrix_read_count() << " scan_min_reads=" << kcfg.scan_min_total_reads
			  << " scan_min_pb_deltas=" << kcfg.scan_min_pb_deltas << ")\n";
		return 1;
	}
	if (m.matrix_read_count() < std::uint64_t(kcfg.scan_min_total_reads)) {
		std::cerr << "matrix read count below board profile minimum\n";
		return 1;
	}
	return 0;
}
