// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_io_shared.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

bool parse_hex_byte(std::string const &s, std::uint8_t &out)
{
	if (s.size() < 4 || s[0] != '0')
		return false;
	if (s[1] != 'x' && s[1] != 'X')
		return false;
	int hi = std::tolower(static_cast<unsigned char>(s[2]));
	int lo = std::tolower(static_cast<unsigned char>(s[3]));
	auto val = [](int c) -> int {
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'a' && c <= 'f')
			return 10 + (c - 'a');
		return -1;
	};
	if (val(hi) < 0 || val(lo) < 0)
		return false;
	out = std::uint8_t((val(hi) << 4) | val(lo));
	return true;
}

bool extract_unknown_default(std::string const &j, std::uint8_t &out)
{
	auto p = j.find("\"unknown_default\"");
	if (p == std::string::npos)
		return false;
	auto colon = j.find(':', p);
	if (colon == std::string::npos)
		return false;
	auto q1 = j.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto q2 = j.find('"', q1 + 1);
	if (q2 == std::string::npos)
		return false;
	return parse_hex_byte(j.substr(q1 + 1, q2 - q1 - 1), out);
}

} // namespace

bool millennium_io_parse_port_map(std::string const &json_text, std::uint8_t &unknown_default_out,
	std::array<bool, 256> &known_or_suspected_out, std::string &error_out)
{
	known_or_suspected_out.fill(false);
	if (!extract_unknown_default(json_text, unknown_default_out)) {
		error_out = "millennium_io_parse_port_map: unknown_default not found";
		return false;
	}

	for (std::size_t i = 0; i < json_text.size(); ++i) {
		auto port_key = json_text.find("\"port\"", i);
		if (port_key == std::string::npos)
			break;
		auto status_key = json_text.find("\"status\"", port_key);
		if (status_key == std::string::npos || status_key - port_key > 400) {
			i = port_key + 1;
			continue;
		}
		auto q1 = json_text.find("\"0x", port_key);
		if (q1 == std::string::npos || q1 > port_key + 80) {
			i = port_key + 1;
			continue;
		}
		auto q2 = json_text.find('"', q1 + 1);
		if (q2 == std::string::npos || q2 - q1 - 1 != 4) {
			i = port_key + 1;
			continue;
		}
		std::string port_lit = json_text.substr(q1 + 1, q2 - q1 - 1);
		std::uint8_t portv = 0;
		if (!parse_hex_byte(port_lit, portv)) {
			i = port_key + 1;
			continue;
		}
		auto sq1 = json_text.find('"', status_key + 8);
		if (sq1 == std::string::npos) {
			i = port_key + 1;
			continue;
		}
		auto sq2 = json_text.find('"', sq1 + 1);
		if (sq2 == std::string::npos) {
			i = port_key + 1;
			continue;
		}
		std::string st = json_text.substr(sq1 + 1, sq2 - sq1 - 1);
		if (st == "known" || st == "suspected")
			known_or_suspected_out[portv] = true;
		i = status_key + 1;
	}
	return true;
}

std::string millennium_format_unknown_port_json(std::string const &ts_rfc3339, std::uint64_t cycle,
	std::uint16_t pc, std::uint16_t port_full, bool is_write, std::uint8_t value, char const *source_symbol,
	char const *note)
{
	char pcbuf[16];
	char port16buf[16];
	char portlobuf[16];
	char valbuf[16];
	std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
	std::snprintf(port16buf, sizeof(port16buf), "0x%04X", unsigned(port_full));
	std::snprintf(portlobuf, sizeof(portlobuf), "0x%02X", unsigned(port_full & 0xffU));
	std::snprintf(valbuf, sizeof(valbuf), "0x%02X", unsigned(value));
	std::ostringstream os;
	os << '{';
	os << "\"ts\":\"" << ts_rfc3339 << "\",";
	os << "\"cycle\":" << cycle << ',';
	os << "\"pc\":\"" << pcbuf << "\",";
	os << "\"port16\":\"" << port16buf << "\",";
	os << "\"port_lo\":\"" << portlobuf << "\",";
	os << "\"rw\":\"" << (is_write ? 'w' : 'r') << "\",";
	os << "\"value\":\"" << valbuf << "\",";
	os << "\"source_symbol\":";
	if (source_symbol && *source_symbol)
		os << '"' << source_symbol << '"';
	else
		os << "null";
	os << ',';
	os << "\"note\":\"" << (note ? note : "") << '"';
	os << '}';
	return os.str();
}

bool coinline_env_value_is_truthy(char const *value)
{
	if (!value || !std::strlen(value))
		return false;
	std::string v;
	for (char const *p = value; *p; ++p)
		v.push_back(char(std::tolower(static_cast<unsigned char>(*p))));
	return v == "1" || v == "true" || v == "yes" || v == "on";
}

bool coinline_env_value_is_falsey(char const *value)
{
	if (!value || !std::strlen(value))
		return false;
	std::string v;
	for (char const *p = value; *p; ++p)
		v.push_back(char(std::tolower(static_cast<unsigned char>(*p))));
	return v == "0" || v == "false" || v == "no" || v == "off";
}

bool coinline_env_default_true_unless_falsey(char const *value)
{
	if (!value || !std::strlen(value))
		return true;
	return !coinline_env_value_is_falsey(value);
}
