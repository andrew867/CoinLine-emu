#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

inline std::string coinline_read_all_text(std::filesystem::path const &p)
{
	std::ifstream in(p, std::ios::binary);
	if (!in)
		throw std::runtime_error("open failed: " + p.string());
	std::ostringstream os;
	os << in.rdbuf();
	std::string s = os.str();
	// Normalize CRLF in fixture files on Windows checkouts; VFD buffer bytes are unchanged.
	s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
	return s;
}

inline std::vector<std::uint8_t> vfd_parse_hex_raw_field(std::string const &json)
{
	auto const key = std::string("\"raw\":\"");
	auto p = json.find(key);
	if (p == std::string::npos)
		throw std::runtime_error("raw field not found");
	p += key.size();
	std::string hexpart;
	while (p < json.size()) {
		char c = json[p];
		if (c == '"' && (p == 0 || json[p - 1] != '\\'))
			break;
		hexpart.push_back(c);
		++p;
	}
	std::vector<std::uint8_t> out;
	std::size_t i = 0;
	while (i < hexpart.size()) {
		while (i < hexpart.size() && std::isspace(static_cast<unsigned char>(hexpart[i])))
			++i;
		if (i >= hexpart.size())
			break;
		if (i + 4 > hexpart.size() || hexpart[i] != '0' || (hexpart[i + 1] != 'x' && hexpart[i + 1] != 'X'))
			throw std::runtime_error("malformed hex token");
		i += 2;
		int hi = std::tolower(static_cast<unsigned char>(hexpart[i++]));
		int lo = std::tolower(static_cast<unsigned char>(hexpart[i++]));
		auto val = [](int x) -> int {
			if (x >= '0' && x <= '9')
				return x - '0';
			if (x >= 'a' && x <= 'f')
				return x - 'a' + 10;
			return -1;
		};
		int vh = val(hi);
		int vl = val(lo);
		if (vh < 0 || vl < 0)
			throw std::runtime_error("bad hex digit");
		out.push_back(static_cast<std::uint8_t>((vh << 4) | vl));
	}
	return out;
}
