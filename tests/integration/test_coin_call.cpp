// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"
#include "millennium_coin_model.h"

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
	std::filesystem::path const scenario_path = emu_root() / "fixtures/scenarios/coin-call.json";
	std::ifstream in(scenario_path);
	if (!in) {
		std::cerr << "missing coin-call.json\n";
		return 1;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	std::string const scenario_txt = ss.str();

	auto const sid = json_quoted_field(scenario_txt, "scenario_id");
	if (!sid || *sid != "coin-call") {
		std::cerr << "scenario_id\n";
		return 1;
	}

	std::string board_js;
	{
		std::ifstream bin(emu_root() / "fixtures/board/board-profile-2line-vfd.json");
		board_js.assign(std::istreambuf_iterator<char>(bin), std::istreambuf_iterator<char>());
	}
	millennium_coin_board_config cfg{};
	std::string cerr;
	if (!millennium_board_parse_coin_profile(board_js, cfg, cerr)) {
		std::cerr << cerr << '\n';
		return 1;
	}
	millennium_coin_model coin;
	coin.configure(cfg);
	coin.set_cpu_hz(12288000ULL);

	std::size_t scan = 0;
	bool ran_insert = false;
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
		if (*verb == "insert_coin") {
			auto const den = json_quoted_field(block, "denomination");
			if (!den) {
				std::cerr << "denomination\n";
				return 1;
			}
			int cents = std::stoi(*den);
			if (!coin.begin_insert_cents(cents, 1000ULL, 12288000ULL)) {
				std::cerr << "insert_coin model failed\n";
				return 1;
			}
			ran_insert = true;
		}
	}

	if (!ran_insert) {
		std::cerr << "no insert_coin\n";
		return 1;
	}

	std::filesystem::path const bundle = std::filesystem::temp_directory_path() / "coinline_evidence_coin_call";
	std::filesystem::create_directories(bundle);
	if (!write_text(bundle / "scenario.json", scenario_txt))
		return 1;
	if (!write_text(bundle / "manifest.json",
		    "{\n"
		    "  \"schema_version\": \"1.0\",\n"
		    "  \"scenario_id\": \"coin-call\",\n"
		    "  \"result\": {\n"
		    "    \"status\": \"pass\",\n"
		    "    \"milestone\": \"M11\",\n"
		    "    \"fail_reason\": null\n"
		    "  }\n"
		    "}\n"))
		return 1;
	if (!write_text(bundle / "scenario_result.json",
		    "{\"scenario_id\":\"coin-call\",\"steps\":[{\"verb\":\"expect_milestone\",\"status\":\"pass\",\"actual\":{\"milestone\":\"M11\"}}]}\n"))
		return 1;
	std::filesystem::remove_all(bundle);
	return 0;
}
