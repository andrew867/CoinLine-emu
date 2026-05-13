// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voiceware_phrase_port.h"

#include <cctype>
#include <cstring>

bool coinline_voiceware_parse_hex_u8(char const *s, std::uint8_t &out)
{
	if (!s || !*s)
		return false;
	while (*s == ' ' || *s == '\t')
		++s;
	if (!*s)
		return false;
	unsigned v = 0;
	unsigned digits = 0;
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		if (!*s)
			return false;
	}
	for (; *s; ++s) {
		int c = std::tolower(static_cast<unsigned char>(*s));
		if (c == ' ' || c == '\t')
			break;
		unsigned n = 0;
		if (c >= '0' && c <= '9')
			n = unsigned(c - '0');
		else if (c >= 'a' && c <= 'f')
			n = 10U + unsigned(c - 'a');
		else
			return false;
		v = (v << 4) | n;
		++digits;
		if (digits > 2)
			return false;
	}
	if (digits == 0)
		return false;
	out = static_cast<std::uint8_t>(v & 0xffU);
	return true;
}

void coinline_voiceware_phrase_port_levels_apply_hex_env(coinline_voiceware_phrase_port_levels &levels,
	char const *env_idle, char const *env_busy, char const *env_fault)
{
	std::uint8_t v = 0;
	if (env_idle && coinline_voiceware_parse_hex_u8(env_idle, v))
		levels.idle_read = v;
	if (env_busy && coinline_voiceware_parse_hex_u8(env_busy, v))
		levels.busy_read = v;
	if (env_fault && coinline_voiceware_parse_hex_u8(env_fault, v))
		levels.fault_read = v;
}
