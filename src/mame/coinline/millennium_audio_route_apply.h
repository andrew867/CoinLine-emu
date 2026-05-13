// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>

/// Pure routing state + command decode per `fixtures/board/audio-routing-state-map.json`.
/// Used by `millennium_audio_route_device` and CoinLine unit tests (coinline_support).

namespace coinline::audio_route {

enum class call_state : std::uint8_t {
	CALL_IDLE,
	CALL_DIALING,
	CALL_ESTAB_UNSUPVSED,
	CALL_ESTAB_INCOMING,
	CALL_ESTAB_SUPVSED,
	CALL_ESTAB_UNSVD_CHARGE,
	CALL_ESTAB_SVD_CHARGE
};

struct composite_state {
	call_state call = call_state::CALL_IDLE;
	bool tx_muted = false;
	bool rx_muted = false;
	bool sidetone_suppressed = false;
	bool hook_off = false;
	bool voice_prompt_path = false;
	bool modem_carrier_up = false;
	/// RX gain step index 0–3 for preset commands (comment-only semantics).
	unsigned rx_gain_step = 0U;
};

/// Lookup command byte; returns false if not in routing map (no trace decode).
bool lookup_command(std::uint8_t b, char const *&out_label, char const *&out_effect);

/// Apply one telephony-processor command effect string (from map `effect` field).
/// Mutates \a s; returns true if \a effect was recognized.
bool apply_effect(composite_state &s, char const *effect, std::string *notes_out);

/// Snapshot strings for traces / fixtures (`route_state` + mute tuple).
void snapshot_route_string(composite_state const &s, std::string &out_route);
void snapshot_mute_json(composite_state const &s, std::string &out_json_object);

/// After `device_reset` / power-on — must match `fixtures/audio/audio-route-idle.json`.
void reset_to_idle_fixture(composite_state &s);

bool call_state_established(call_state c) noexcept;

/// Integration label for optional `call_state_if_known` trace field (`audio-call-state-integration.spec.md`).
/// Empty string when not confidently derivable from composite hardware state alone.
void snapshot_integration_call_state(composite_state const &s, std::string &out);

} // namespace coinline::audio_route
