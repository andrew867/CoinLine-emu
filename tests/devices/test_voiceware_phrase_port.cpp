// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voiceware_phrase_port.h"

int main()
{
	std::uint8_t v = 0;
	if (!coinline_voiceware_parse_hex_u8("0xFF", v) || v != 0xffU)
		return 1;
	if (!coinline_voiceware_parse_hex_u8("7f", v) || v != 0x7fU)
		return 2;
	if (!coinline_voiceware_parse_hex_u8("0x0A", v) || v != 0x0aU)
		return 3;
	if (coinline_voiceware_parse_hex_u8("", v) || coinline_voiceware_parse_hex_u8("GG", v))
		return 4;

	coinline_voiceware_phrase_port_levels lv{};
	if (lv.idle_read != 0xffU || lv.busy_read != 0x7fU)
		return 5;
	coinline_voiceware_phrase_port_levels_apply_hex_env(lv, "0xFE", nullptr, "2A");
	if (lv.idle_read != 0xfeU || lv.busy_read != 0x7fU || lv.fault_read != 0x2aU)
		return 6;
	return 0;
}
