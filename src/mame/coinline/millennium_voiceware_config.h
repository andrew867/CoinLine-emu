// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_voiceware_phrase_port.h"

/// Environment-driven toggles for Voiceware / uPD7759 integration (MAME driver only).
///
/// `COINLINE_VOICEWARE_UPD7759_CORE` — skip ROM header patch; use chip playback path (**default on**; set `0`/`off`/`false`/`no` to disable).
/// `COINLINE_VOICEWARE_LEGACY_FALLBACK` — when core is on and phrase lookup fails, run software decoder (default off).
/// `COINLINE_VOICEWARE_ANALOG_BYPASS` — disable low-pass `FILTER_RC` between uPD and speaker (default off).
/// `COINLINE_VOICEWARE_ANALOG_GAIN` — multiplier on route to `vwspk` (default 1.0, max 4).
/// `COINLINE_VOICEWARE_0x61_IDLE` / `_BUSY` / `_FAULT` — hex byte overrides for `0x61` reads (datasheet / bench).
/// `COINLINE_VOICEWARE_RETRIGGER_POLICY` — `suppress` (default) or `allow` duplicate phrase strobes while playing.

enum class coinline_voiceware_retrigger_policy : std::uint8_t {
	/// Ignore `0x61` write when same phrase+bank and playback still active (firmware continuity).
	suppress_duplicate_strobe_while_playing = 0,
	/// Always run phrase start path (uPD `port_w` + start); for lab / future datasheet “restart” models.
	allow_duplicate_strobe = 1,
};

bool coinline_voiceware_upd7759_core_from_env();
bool coinline_voiceware_legacy_fallback_from_env();
bool coinline_voiceware_analog_filter_bypass_from_env();
float coinline_voiceware_analog_route_gain_from_env();

void coinline_voiceware_phrase_port_levels_from_osd(coinline_voiceware_phrase_port_levels &out);
coinline_voiceware_retrigger_policy coinline_voiceware_retrigger_policy_from_env();
