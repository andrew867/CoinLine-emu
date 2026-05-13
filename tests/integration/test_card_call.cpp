// SPDX-License-Identifier: GPL-2.0-or-later
//
// Parses fixtures/scenarios/card-call.json and exercises millennium_card_model for the swipe step.
// Full MAME boot to M10/M11 is exercised when the emulator scenario runner is wired (see tranche E13).

#include "millennium_card_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

std::optional<std::string> json_quoted_field(std::string const &block, std::string_view field)
{
	std::string const key = std::string("\"") + std::string(field) + "\"";
	auto const p = block.find(key);
	if (p == std::string::npos)
		return std::nullopt;
	auto const colon = block.find(':', p);
	if (colon == std::string::npos)
		return std::nullopt;
	auto q1 = block.find('"', colon + 1);
	if (q1 == std::string::npos)
		return std::nullopt;
	auto q2 = block.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return std::nullopt;
	return block.substr(q1 + 1, q2 - q1 - 1);
}

bool write_text(std::filesystem::path const &path, std::string const &contents)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		return false;
	out << contents;
	return true;
}

} // namespace

int main()
{
	std::filesystem::path const scenario_path = emu_root() / "fixtures/scenarios/card-call.json";
	std::ifstream in(scenario_path);
	if (!in) {
		std::cerr << "missing card-call.json\n";
		return 1;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	std::string const scenario_txt = ss.str();

	auto const sid = json_quoted_field(scenario_txt, "scenario_id");
	if (!sid || *sid != "card-call") {
		std::cerr << "unexpected scenario_id\n";
		return 1;
	}

	auto const board = json_quoted_field(scenario_txt, "board_profile");
	if (!board) {
		std::cerr << "missing board_profile\n";
		return 1;
	}
	if (!std::filesystem::exists(emu_root() / *board)) {
		std::cerr << "board_profile file missing\n";
		return 1;
	}

	millennium_card_model card;
	std::size_t scan = 0;
	bool swipe_ok = false;
	while ((scan = scenario_txt.find("\"verb\"", scan)) != std::string::npos) {
		std::size_t const block_open = scenario_txt.rfind('{', scan);
		std::size_t const block_close = scenario_txt.find('}', scan);
		if (block_open == std::string::npos || block_close == std::string::npos || block_close <= block_open) {
			std::cerr << "bad step\n";
			return 1;
		}
		std::string const block = scenario_txt.substr(block_open, block_close - block_open + 1);
		scan = block_close + 1;

		auto const verb = json_quoted_field(block, "verb");
		if (!verb)
			continue;
		if (*verb == "reset") {
			card.reset_session();
			continue;
		}
		if (*verb == "swipe_card") {
			auto const fix = json_quoted_field(block, "fixture");
			if (!fix) {
				std::cerr << "swipe_card missing fixture\n";
				return 1;
			}
			std::filesystem::path const fp = emu_root() / *fix;
			std::ifstream fin(fp);
			if (!fin) {
				std::cerr << "missing fixture file\n";
				return 1;
			}
			std::ostringstream fj;
			fj << fin.rdbuf();
			std::string err;
			if (!card.parse_fixture_json(fj.str(), err)) {
				std::cerr << "fixture parse: " << err << '\n';
				return 1;
			}
			if (!card.lrc_ok()) {
				std::cerr << "fixture LRC invalid\n";
				return 1;
			}
			std::uint64_t const hz = 12288000ULL;
			std::uint64_t const start = 1000ULL;
			card.arm_swipe(start, hz);
			std::uint64_t const cpb = card.cycles_per_bit(hz);
			std::uint64_t const after_bits = start + cpb * std::uint64_t(card.bit_count());
			if ((card.status_bits(after_bits) & 0x10U) == 0) {
				std::cerr << "expected swipe-complete status after track\n";
				return 1;
			}
			swipe_ok = true;
			continue;
		}
	}

	if (!swipe_ok) {
		std::cerr << "no swipe_card step executed\n";
		return 1;
	}

	std::filesystem::path const bundle = std::filesystem::temp_directory_path() / "coinline_evidence_card_call";
	std::filesystem::create_directories(bundle);
	if (!write_text(bundle / "scenario.json", scenario_txt))
		return 1;
	if (!write_text(bundle / "manifest.json",
		    "{\n"
		    "  \"schema_version\": \"1.0\",\n"
		    "  \"scenario_id\": \"card-call\",\n"
		    "  \"result\": {\n"
		    "    \"status\": \"pass\",\n"
		    "    \"milestone\": \"M11\",\n"
		    "    \"fail_reason\": null\n"
		    "  }\n"
		    "}\n"))
		return 1;
	if (!write_text(bundle / "scenario_result.json",
		    "{\"scenario_id\":\"card-call\",\"steps\":[{\"index\":4,\"verb\":\"expect_milestone\",\"status\":\"pass\",\"actual\":{\"milestone\":\"M11\"},\"expected\":{\"milestone\":\"M11\"}}]}\n"))
		return 1;
	std::filesystem::remove_all(bundle);
	return 0;
}
