// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline std::optional<std::vector<std::uint8_t>> coinline_base64_decode(std::string_view in)
{
	static const signed char kMap[256] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
		-1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
		-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	};
	std::vector<std::uint8_t> out;
	out.reserve(in.size() * 3 / 4);
	int val = 0, valb = -8;
	for (unsigned char c : in) {
		if (std::isspace(c))
			continue;
		if (c == '=')
			break;
		unsigned const ix = static_cast<unsigned>(c);
		if (ix >= sizeof(kMap) / sizeof(kMap[0]) || kMap[ix] < 0)
			return std::nullopt;
		val = (val << 6) + static_cast<int>(kMap[ix]);
		valb += 6;
		if (valb >= 0) {
			out.push_back(static_cast<std::uint8_t>((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

inline std::optional<std::string> coinline_base64_decode_string(std::string_view in)
{
	auto b = coinline_base64_decode(in);
	if (!b)
		return std::nullopt;
	return std::string(reinterpret_cast<char const *>(b->data()), b->size());
}
