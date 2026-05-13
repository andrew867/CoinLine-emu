// SPDX-License-Identifier: GPL-2.0-or-later
// Class A: replay `replay_steps` / `expected_trace` from fixtures/audio/disconnect-*.json against FSM.

#include "millennium_supervision_fsm.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

bool read_all(std::filesystem::path const &p, std::string &out)
{
	std::ifstream in(p, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

std::optional<std::string> extract_array(std::string const &json, char const *key)
{
	std::string const needle = std::string("\"") + key + "\"";
	auto p = json.find(needle);
	if (p == std::string::npos)
		return std::nullopt;
	auto lb = json.find('[', p);
	if (lb == std::string::npos)
		return std::nullopt;
	int depth = 0;
	for (std::size_t i = lb; i < json.size(); ++i) {
		if (json[i] == '[')
			++depth;
		else if (json[i] == ']') {
			--depth;
			if (depth == 0)
				return json.substr(lb, i - lb + 1);
		}
	}
	return std::nullopt;
}

std::vector<std::string> split_top_objects(std::string const &array_brackets)
{
	std::vector<std::string> out;
	if (array_brackets.size() < 2 || array_brackets.front() != '[' || array_brackets.back() != ']')
		return out;
	std::string const inner = array_brackets.substr(1, array_brackets.size() - 2);
	std::size_t i = 0;
	while (i < inner.size()) {
		while (i < inner.size() && (inner[i] == ',' || std::isspace(static_cast<unsigned char>(inner[i]))))
			++i;
		if (i >= inner.size() || inner[i] != '{')
			break;
		int depth = 0;
		std::size_t const start = i;
		for (; i < inner.size(); ++i) {
			if (inner[i] == '{')
				++depth;
			else if (inner[i] == '}') {
				--depth;
				if (depth == 0) {
					++i;
					out.push_back(inner.substr(start, i - start));
					break;
				}
			}
		}
	}
	return out;
}

std::optional<std::string> quoted_field(std::string const &obj, char const *key)
{
	std::string pat = std::string("\"") + key + "\"";
	auto p = obj.find(pat);
	if (p == std::string::npos)
		return std::nullopt;
	auto colon = obj.find(':', p);
	if (colon == std::string::npos)
		return std::nullopt;
	auto q1 = obj.find('"', colon + 1);
	if (q1 == std::string::npos)
		return std::nullopt;
	auto q2 = obj.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return std::nullopt;
	return obj.substr(q1 + 1, q2 - q1 - 1);
}

std::optional<bool> bool_field(std::string const &obj, char const *key)
{
	std::string const pat = std::string("\"") + key + "\"";
	auto const p = obj.find(pat);
	if (p == std::string::npos)
		return std::nullopt;
	auto const colon = obj.find(':', p);
	if (colon == std::string::npos)
		return std::nullopt;
	std::size_t i = colon + 1;
	while (i < obj.size() && std::isspace(static_cast<unsigned char>(obj[i])))
		++i;
	if (i + 4 <= obj.size() && obj.compare(i, 4, "true") == 0)
		return true;
	if (i + 5 <= obj.size() && obj.compare(i, 5, "false") == 0)
		return false;
	return std::nullopt;
}

unsigned parse_hex_byte(std::string const &h)
{
	return static_cast<unsigned>(std::strtoul(h.c_str(), nullptr, 0));
}

char const *disconnect_str(coinline::supervision::disconnect_kind k)
{
	using coinline::supervision::disconnect_kind;
	switch (k) {
	case disconnect_kind::normal_disconnect:
		return "normal_disconnect";
	case disconnect_kind::cpc:
		return "cpc";
	case disconnect_kind::timeout:
		return "timeout";
	case disconnect_kind::fault:
		return "fault";
	default:
		return "none";
	}
}

bool run_fixture(char const *rel)
{
	std::string json;
	if (!read_all(emu_root() / rel, json)) {
		std::cerr << "cannot read " << rel << '\n';
		return false;
	}
	auto const arr_rep = extract_array(json, "replay_steps");
	auto const arr_exp = extract_array(json, "expected_trace");
	if (!arr_rep || !arr_exp) {
		std::cerr << rel << ": missing replay_steps or expected_trace\n";
		return false;
	}
	auto const steps = split_top_objects(*arr_rep);
	auto const expected_objs = split_top_objects(*arr_exp);

	coinline::supervision::disconnect_supervision_fsm fsm;
	fsm.reset();
	std::vector<coinline::supervision::supervision_fsm_event> got;

	for (std::string const &so : steps) {
		auto const kind = quoted_field(so, "kind");
		if (!kind)
			continue;
		std::uint64_t const cy = static_cast<std::uint64_t>(got.size() + 1u);
		std::uint16_t const pc = 0x4000;
		if (*kind == "carrier") {
			auto up = bool_field(so, "up");
			if (!up)
				return false;
			auto v = fsm.step_carrier(*up, cy, pc);
			got.insert(got.end(), v.begin(), v.end());
		}
		else if (*kind == "status") {
			auto hx = quoted_field(so, "hex");
			if (!hx)
				return false;
			unsigned const b = parse_hex_byte(*hx);
			auto v = fsm.step_status(static_cast<std::uint8_t>(b), cy, pc);
			got.insert(got.end(), v.begin(), v.end());
		}
		else if (*kind == "watchdog_fired") {
			auto v = fsm.step_watchdog_fired(cy, pc);
			got.insert(got.end(), v.begin(), v.end());
		}
	}

	if (got.size() != expected_objs.size()) {
		std::cerr << rel << ": event count got=" << got.size() << " expected=" << expected_objs.size() << '\n';
		return false;
	}
	for (std::size_t i = 0; i < got.size(); ++i) {
		auto const &e = got[i];
		std::string const &exp = expected_objs[i];
		auto const et = quoted_field(exp, "event_type");
		if (!et) {
			std::cerr << rel << ": expected object missing event_type\n";
			return false;
		}
		if (e.kind == coinline::supervision::supervision_fsm_event::kind::supervision_status_code) {
			if (*et != "supervision_status_code")
				return false;
			auto hx = quoted_field(exp, "status_code_hex");
			if (!hx || parse_hex_byte(*hx) != unsigned(e.status_code))
				return false;
		}
		else if (e.kind == coinline::supervision::supervision_fsm_event::kind::disconnect_event) {
			if (*et != "disconnect_event")
				return false;
			auto de = quoted_field(exp, "disconnect_event");
			if (!de || *de != disconnect_str(e.disconnect))
				return false;
		}
		else if (e.kind == coinline::supervision::supervision_fsm_event::kind::supervision_timeout) {
			if (*et != "supervision_timeout")
				return false;
		}
		else {
			return false;
		}
	}
	return true;
}

} // namespace

int main()
{
	if (!run_fixture("fixtures/audio/disconnect-normal.json"))
		return 1;
	if (!run_fixture("fixtures/audio/disconnect-cpc.json"))
		return 1;
	if (!run_fixture("fixtures/audio/disconnect-timeout.json"))
		return 1;
	return 0;
}
