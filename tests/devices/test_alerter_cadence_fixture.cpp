// SPDX-License-Identifier: GPL-2.0-or-later
// Class A: cadence edge tables match fixtures/audio/alerter-*.json.

#include "millennium_audio_model.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool read_file(std::filesystem::path const &p, std::string &out)
{
	std::ifstream in(p, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

bool parse_quoted_field(std::string const &json, char const *key, std::string &value_out)
{
	std::string const pat = std::string("\"") + key + "\"";
	auto const p = json.find(pat);
	if (p == std::string::npos)
		return false;
	auto const colon = json.find(':', p);
	if (colon == std::string::npos)
		return false;
	auto const q1 = json.find('"', colon + 1);
	if (q1 == std::string::npos)
		return false;
	auto const q2 = json.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return false;
	value_out = json.substr(q1 + 1, q2 - q1 - 1);
	return true;
}

bool parse_edges_ms_array(std::string const &json, std::vector<unsigned> &out)
{
	auto const keypos = json.find("\"edges_ms\"");
	if (keypos == std::string::npos)
		return false;
	auto const lb = json.find('[', keypos);
	auto const rb = json.find(']', lb);
	if (lb == std::string::npos || rb == std::string::npos || rb <= lb)
		return false;
	std::string const inner = json.substr(lb + 1, rb - lb - 1);
	std::istringstream iss(inner);
	unsigned v = 0;
	char c = 0;
	while (iss >> v) {
		out.push_back(v);
		iss >> c;
	}
	return !out.empty();
}

millennium_alerter_cadence_kind kind_from_fixture(std::string const &s)
{
	if (s == "service_beep")
		return millennium_alerter_cadence_kind::service_beep;
	if (s == "error_beep")
		return millennium_alerter_cadence_kind::error_beep;
	if (s == "user_prompt_tone")
		return millennium_alerter_cadence_kind::user_prompt_tone;
	return millennium_alerter_cadence_kind::none;
}

bool check_fixture(char const *rel_path)
{
	std::string raw;
	if (!read_file(emu_root() / rel_path, raw)) {
		std::cerr << "missing " << rel_path << '\n';
		return false;
	}
	std::string tone_class;
	if (!parse_quoted_field(raw, "tone_class", tone_class)) {
		std::cerr << "tone_class not found in " << rel_path << '\n';
		return false;
	}
	millennium_alerter_cadence_kind const k = kind_from_fixture(tone_class);
	if (k == millennium_alerter_cadence_kind::none) {
		std::cerr << "unknown tone_class " << tone_class << '\n';
		return false;
	}
	std::vector<unsigned> edges;
	if (!parse_edges_ms_array(raw, edges)) {
		std::cerr << "edges_ms not parsed in " << rel_path << '\n';
		return false;
	}
	unsigned const n = millennium_audio_model::cadence_edge_count(k);
	if (n != edges.size()) {
		std::cerr << rel_path << ": edge count mismatch model=" << n << " fixture=" << edges.size() << '\n';
		return false;
	}
	for (unsigned i = 0; i < n; ++i) {
		unsigned const m = millennium_audio_model::cadence_edge_ms(k, i);
		if (m != edges[i]) {
			std::cerr << rel_path << ": edge[" << i << "] model=" << m << " fixture=" << edges[i] << '\n';
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	if (!check_fixture("fixtures/audio/alerter-service-beep.json"))
		return 1;
	if (!check_fixture("fixtures/audio/alerter-error-beep.json"))
		return 1;
	return 0;
}
