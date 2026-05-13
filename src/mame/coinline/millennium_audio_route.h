// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_audio_route_apply.h"

#include <string>

DECLARE_DEVICE_TYPE(MILLENNIUM_AUDIO_ROUTE, millennium_audio_route_device)

/// Applies telephony routing map effects, coordinates voiceware / hook / modem (see spec).
/// Route traces may carry **`call_state_if_known`** derived from this device's **`composite_state`** only.
class millennium_audio_route_device : public device_t {
public:
	millennium_audio_route_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void notify_voice_active(bool active) noexcept;
	void notify_hook_off(bool off_hook) noexcept;
	void notify_modem_carrier(bool carrier_up) noexcept;

	/// Apply one decoded `effect` string from `audio-routing-state-map.json` (telephony device calls this).
	void apply_telephony_effect(char const *effect, std::uint8_t raw_byte, std::uint64_t cycle, std::uint16_t pc,
		char const *command_label);

	coinline::audio_route::composite_state const &route_state() const noexcept { return m_st; }

private:
	void emit_route_change(std::uint64_t cycle, std::uint16_t pc, char const *reason);
	void emit_mute_change(std::uint64_t cycle, std::uint16_t pc);
	void maybe_emit_route_conflict(std::uint64_t cycle, std::uint16_t pc);
	void run_voice_prompt_conditioning(bool voice_now);
	std::uint64_t machine_cycle_approx() const;

	coinline::audio_route::composite_state m_st{};
	std::string m_prev_route_str;
	std::string m_prev_mute_json;
	bool m_voice_active = false;
};
