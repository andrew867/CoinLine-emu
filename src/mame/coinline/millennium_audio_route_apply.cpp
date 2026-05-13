// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_route_apply.h"

#include <cstring>
#include <sstream>

namespace coinline::audio_route {

namespace {

struct cmd_entry {
	std::uint8_t byte;
	char const *label;
	char const *effect;
};

// Mirrors `fixtures/board/audio-routing-state-map.json` commands{} — single source comment.
cmd_entry const k_commands[] = {
	{ 0x20, "PRESET_VOLUME_LEVEL_1", "rx_gain_preset_0db" },
	{ 0x21, "PRESET_VOLUME_LEVEL_2", "rx_gain_preset_6_7db_comment" },
	{ 0x22, "PRESET_VOLUME_LEVEL_3", "rx_gain_preset_13_3db_comment" },
	{ 0x23, "PRESET_VOLUME_LEVEL_4", "rx_gain_preset_20db_comment" },
	{ 0x24, "VOLUME_LEVEL_DOWN", "rx_gain_step_down" },
	{ 0x25, "VOLUME_LEVEL_UP", "rx_gain_step_up" },
	{ 0x26, "TX_UNMUTE", "handset_tx_unmute" },
	{ 0x27, "TX_MUTE", "handset_tx_mute" },
	{ 0x28, "RX_UNMUTE", "handset_rx_unmute" },
	{ 0x29, "RX_MUTE", "handset_rx_mute" },
	{ 0x2a, "SIDETONE_SUPPRESSION_OFF", "sidetone_suppression_off" },
	{ 0x2b, "SIDETONE_SUPPRESSION_ON", "sidetone_suppression_on" },
	{ 0x2d, "DIAL_PAD_DTMF_DISABLE", "keypad_dtmf_disable" },
	{ 0x2e, "DIAL_PAD_DTMF_ENABLE", "keypad_dtmf_enable" },
	{ 0x2f, "TONE_GENERATOR_OFF", "tone_generator_off" },
	{ 0x40, "CALL_STATE_IDLE", "call_state_idle" },
	{ 0x41, "CALL_STATE_DIALING", "call_state_dialing" },
	{ 0x42, "CALL_STATE_ESTAB_UNSUPVSED", "call_state_established_unsupervised" },
	{ 0x43, "CALL_STATE_ESTAB_INCOMING", "call_state_established_incoming" },
	{ 0x44, "CALL_STATE_ESTAB_SUPVSED", "call_state_established_supervised" },
	{ 0x45, "CALL_STATE_ESTAB_UNSVD_CHARGE", "call_state_established_unsupervised_charge" },
	{ 0x46, "CALL_STATE_ESTAB_SVD_CHARGE", "call_state_established_supervised_charge" },
};

} // namespace

bool call_state_established(call_state c) noexcept
{
	return c == call_state::CALL_ESTAB_UNSUPVSED || c == call_state::CALL_ESTAB_INCOMING ||
		c == call_state::CALL_ESTAB_SUPVSED || c == call_state::CALL_ESTAB_UNSVD_CHARGE ||
		c == call_state::CALL_ESTAB_SVD_CHARGE;
}

bool lookup_command(std::uint8_t b, char const *&out_label, char const *&out_effect)
{
	for (cmd_entry const &e : k_commands) {
		if (e.byte == b) {
			out_label = e.label;
			out_effect = e.effect;
			return true;
		}
	}
	return false;
}

void reset_to_idle_fixture(composite_state &s)
{
	s.call = call_state::CALL_IDLE;
	s.tx_muted = false;
	s.rx_muted = false;
	s.sidetone_suppressed = false;
	s.hook_off = false;
	s.voice_prompt_path = false;
	s.modem_carrier_up = false;
	s.rx_gain_step = 0U;
}

bool apply_effect(composite_state &s, char const *effect, std::string *notes_out)
{
	if (!effect || !*effect)
		return false;
	if (std::strcmp(effect, "handset_tx_mute") == 0) {
		s.tx_muted = true;
		return true;
	}
	if (std::strcmp(effect, "handset_tx_unmute") == 0) {
		s.tx_muted = false;
		return true;
	}
	if (std::strcmp(effect, "handset_rx_mute") == 0) {
		s.rx_muted = true;
		return true;
	}
	if (std::strcmp(effect, "handset_rx_unmute") == 0) {
		s.rx_muted = false;
		return true;
	}
	if (std::strcmp(effect, "sidetone_suppression_on") == 0) {
		s.sidetone_suppressed = true;
		return true;
	}
	if (std::strcmp(effect, "sidetone_suppression_off") == 0) {
		s.sidetone_suppressed = false;
		return true;
	}
	if (std::strcmp(effect, "call_state_idle") == 0) {
		s.call = call_state::CALL_IDLE;
		return true;
	}
	if (std::strcmp(effect, "call_state_dialing") == 0) {
		s.call = call_state::CALL_DIALING;
		return true;
	}
	if (std::strcmp(effect, "call_state_established_unsupervised") == 0) {
		s.call = call_state::CALL_ESTAB_UNSUPVSED;
		return true;
	}
	if (std::strcmp(effect, "call_state_established_incoming") == 0) {
		s.call = call_state::CALL_ESTAB_INCOMING;
		return true;
	}
	if (std::strcmp(effect, "call_state_established_supervised") == 0) {
		s.call = call_state::CALL_ESTAB_SUPVSED;
		return true;
	}
	if (std::strcmp(effect, "call_state_established_unsupervised_charge") == 0) {
		s.call = call_state::CALL_ESTAB_UNSVD_CHARGE;
		return true;
	}
	if (std::strcmp(effect, "call_state_established_supervised_charge") == 0) {
		s.call = call_state::CALL_ESTAB_SVD_CHARGE;
		return true;
	}
	if (std::strcmp(effect, "rx_gain_preset_0db") == 0) {
		s.rx_gain_step = 0U;
		return true;
	}
	if (std::strcmp(effect, "rx_gain_preset_6_7db_comment") == 0) {
		s.rx_gain_step = 1U;
		if (notes_out)
			*notes_out = "compatibility_validation_required:gain_comment_only";
		return true;
	}
	if (std::strcmp(effect, "rx_gain_preset_13_3db_comment") == 0) {
		s.rx_gain_step = 2U;
		if (notes_out)
			*notes_out = "compatibility_validation_required:gain_comment_only";
		return true;
	}
	if (std::strcmp(effect, "rx_gain_preset_20db_comment") == 0) {
		s.rx_gain_step = 3U;
		if (notes_out)
			*notes_out = "compatibility_validation_required:gain_comment_only";
		return true;
	}
	if (std::strcmp(effect, "rx_gain_step_down") == 0) {
		if (s.rx_gain_step > 0U)
			--s.rx_gain_step;
		return true;
	}
	if (std::strcmp(effect, "rx_gain_step_up") == 0) {
		if (s.rx_gain_step < 3U)
			++s.rx_gain_step;
		return true;
	}
	// DTMF / tone: acknowledged for decode traces; no mux state in this tranche.
	if (std::strcmp(effect, "keypad_dtmf_disable") == 0 || std::strcmp(effect, "keypad_dtmf_enable") == 0 ||
		std::strcmp(effect, "tone_generator_off") == 0)
		return true;
	if (notes_out)
		*notes_out = "compatibility_validation_required:unknown_effect";
	return false;
}

void snapshot_route_string(composite_state const &s, std::string &out_route)
{
	if (s.hook_off && s.voice_prompt_path) {
		out_route = "off_hook_prompt";
		return;
	}
	if (s.call == call_state::CALL_IDLE) {
		out_route = "idle";
		return;
	}
	if (s.call == call_state::CALL_DIALING) {
		out_route = "dialing";
		return;
	}
	// Established / connected family — align with `audio-route-call-connected.json` / muted.
	if (coinline::audio_route::call_state_established(s.call)) {
		out_route = "call_connected";
		return;
	}
	out_route = "idle";
}

void snapshot_mute_json(composite_state const &s, std::string &out_json_object)
{
	std::ostringstream os;
	os << "{\"mic\":" << (s.tx_muted ? "true" : "false") << ",\"earpiece\":" << (s.rx_muted ? "true" : "false") << "}";
	out_json_object = os.str();
}

void snapshot_integration_call_state(composite_state const &s, std::string &out)
{
	out.clear();
	if (s.voice_prompt_path) {
		out.assign("prompt_playback");
		return;
	}
	if (s.call == call_state::CALL_DIALING) {
		out.assign("dialing");
		return;
	}
	if (call_state_established(s.call)) {
		if (s.modem_carrier_up)
			out.assign("connected_call");
		else
			out.assign("disconnect_detected");
		return;
	}
	if (s.hook_off) {
		out.assign("off_hook");
		return;
	}
	out.assign("idle_on_hook");
}

} // namespace coinline::audio_route
