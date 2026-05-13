// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voiceware_config.h"

#include "millennium_io_shared.h"

#include "osdcore.h"

#include <cctype>
#include <cstdlib>
#include <string>

bool coinline_voiceware_upd7759_core_from_env()
{
	// Default **on** for end-to-end runs; set `COINLINE_VOICEWARE_UPD7759_CORE=0` (or `off`/`false`/`no`) for legacy ROM patch + software decode path.
	return coinline_env_default_true_unless_falsey(osd_getenv("COINLINE_VOICEWARE_UPD7759_CORE"));
}

bool coinline_voiceware_legacy_fallback_from_env()
{
	return coinline_env_value_is_truthy(osd_getenv("COINLINE_VOICEWARE_LEGACY_FALLBACK"));
}

bool coinline_voiceware_analog_filter_bypass_from_env()
{
	return coinline_env_value_is_truthy(osd_getenv("COINLINE_VOICEWARE_ANALOG_BYPASS"));
}

float coinline_voiceware_analog_route_gain_from_env()
{
	char const *g = osd_getenv("COINLINE_VOICEWARE_ANALOG_GAIN");
	if (!g || !*g)
		return 1.0f;
	char *end = nullptr;
	double v = std::strtod(g, &end);
	if (end == g || v <= 0.0 || v > 4.0)
		return 1.0f;
	return float(v);
}

void coinline_voiceware_phrase_port_levels_from_osd(coinline_voiceware_phrase_port_levels &out)
{
	out = {};
	coinline_voiceware_phrase_port_levels_apply_hex_env(out, osd_getenv("COINLINE_VOICEWARE_0x61_IDLE"),
		osd_getenv("COINLINE_VOICEWARE_0x61_BUSY"), osd_getenv("COINLINE_VOICEWARE_0x61_FAULT"));
}

coinline_voiceware_retrigger_policy coinline_voiceware_retrigger_policy_from_env()
{
	char const *p = osd_getenv("COINLINE_VOICEWARE_RETRIGGER_POLICY");
	if (!p || !*p)
		return coinline_voiceware_retrigger_policy::suppress_duplicate_strobe_while_playing;
	std::string v;
	for (; *p; ++p)
		v.push_back(char(std::tolower(static_cast<unsigned char>(*p))));
	if (v == "allow" || v == "always" || v == "always_strobe")
		return coinline_voiceware_retrigger_policy::allow_duplicate_strobe;
	return coinline_voiceware_retrigger_policy::suppress_duplicate_strobe_while_playing;
}
