// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/// Extract a quoted JSON string value for `key` from a single-line JSON object (no nested objects in the value).
inline std::optional<std::string> coinline_json_string_field(std::string_view obj, std::string_view key)
{
	std::string const needle = std::string("\"") + std::string(key) + "\":\"";
	auto p = obj.find(needle);
	if (p == std::string_view::npos)
		return std::nullopt;
	p += needle.size();
	std::string out;
	for (std::size_t i = p; i < obj.size(); ++i) {
		char c = obj[i];
		if (c == '\\' && i + 1 < obj.size()) {
			++i;
			out.push_back(obj[i]);
			continue;
		}
		if (c == '"')
			return out;
		out.push_back(c);
	}
	return std::nullopt;
}

inline std::optional<std::string> coinline_json_raw_field(std::string_view obj, std::string_view key)
{
	std::string const needle = std::string("\"") + std::string(key) + "\":";
	auto p = obj.find(needle);
	if (p == std::string_view::npos)
		return std::nullopt;
	p += needle.size();
	while (p < obj.size() && std::isspace(static_cast<unsigned char>(obj[p])))
		++p;
	if (p >= obj.size())
		return std::nullopt;
	if (obj[p] == '"') {
		std::string out;
		for (std::size_t i = p + 1; i < obj.size(); ++i) {
			char c = obj[i];
			if (c == '\\' && i + 1 < obj.size()) {
				++i;
				out.push_back(obj[i]);
				continue;
			}
			if (c == '"')
				return out;
			out.push_back(c);
		}
		return std::nullopt;
	}
	auto start = p;
	while (p < obj.size() && obj[p] != ',' && obj[p] != '}' && obj[p] != ']')
		++p;
	while (p > start && std::isspace(static_cast<unsigned char>(obj[p - 1])))
		--p;
	return std::string(obj.substr(start, p - start));
}

inline bool coinline_json_bool_field(std::string_view obj, std::string_view key, bool &out)
{
	auto v = coinline_json_raw_field(obj, key);
	if (!v)
		return false;
	std::string b = *v;
	while (!b.empty() && (b.back() == '\r' || b.back() == '\n' || std::isspace(static_cast<unsigned char>(b.back()))))
		b.pop_back();
	if (b == "true") {
		out = true;
		return true;
	}
	if (b == "false") {
		out = false;
		return true;
	}
	return false;
}

inline bool coinline_json_uint64_field(std::string_view obj, std::string_view key, std::uint64_t &out)
{
	auto v = coinline_json_raw_field(obj, key);
	if (!v)
		return false;
	try {
		out = std::stoull(*v);
		return true;
	} catch (...) {
		return false;
	}
}
