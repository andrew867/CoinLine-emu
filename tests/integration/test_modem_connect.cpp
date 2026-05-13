// SPDX-License-Identifier: GPL-2.0-or-later
//
// Parses fixtures/scenarios/modem-connect.json and drives millennium_modem_model verbs.
// Full MAME + evidence wiring is separate.

#include "millennium_modem_model.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace {

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

int json_int_field(std::string const &block, std::string_view field, int default_value)
{
	std::string const key = std::string("\"") + std::string(field) + "\"";
	auto const p = block.find(key);
	if (p == std::string::npos)
		return default_value;
	auto const colon = block.find(':', p);
	if (colon == std::string::npos)
		return default_value;
	std::size_t i = colon + 1;
	while (i < block.size() && (block[i] == ' ' || block[i] == '\t'))
		++i;
	int v = 0;
	bool any = false;
	while (i < block.size() && block[i] >= '0' && block[i] <= '9') {
		any = true;
		v = v * 10 + int(block[i] - '0');
		++i;
	}
	return any ? v : default_value;
}

} // namespace

int main()
{
	std::string const path = std::string(COINLINE_EMU_SOURCE_DIR) + "/fixtures/scenarios/modem-connect.json";
	std::ifstream in(path);
	if (!in) {
		std::cerr << "missing modem-connect.json\n";
		return 1;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	std::string const scenario_txt = ss.str();

	auto const sid = json_quoted_field(scenario_txt, "scenario_id");
	if (!sid || *sid != "modem-connect") {
		std::cerr << "unexpected scenario_id\n";
		return 1;
	}

	millennium_modem_model m;
	m.reset();

	std::size_t scan = 0;
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
			m.reset();
			continue;
		}
		if (*verb == "run_cycles") {
			(void)json_int_field(block, "cycles", 0);
			continue;
		}
		if (*verb == "inject_modem_event") {
			auto const ev = json_quoted_field(block, "event");
			if (!ev) {
				std::cerr << "inject_modem_event missing event\n";
				return 1;
			}
			if (!m.inject_event(ev->c_str())) {
				std::cerr << "inject failed: " << *ev << "\n";
				return 1;
			}
			continue;
		}
	}

	if (m.state() != millennium_modem_state::idle) {
		std::cerr << "expected final idle\n";
		return 1;
	}
	return 0;
}
