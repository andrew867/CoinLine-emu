// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_route.h"

#include "millennium_audio_trace.h"

#include "cpu/z180/z180.h"

#include <cstring>

DEFINE_DEVICE_TYPE(MILLENNIUM_AUDIO_ROUTE, millennium_audio_route_device, "millennium_audio_route",
	"CoinLine audio route")

millennium_audio_route_device::millennium_audio_route_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_AUDIO_ROUTE, tag, owner, clock)
{
}

void millennium_audio_route_device::device_start() {}

void millennium_audio_route_device::device_reset()
{
	device_t::device_reset();
	coinline::audio_route::reset_to_idle_fixture(m_st);
	m_voice_active = false;
	m_prev_route_str.clear();
	m_prev_mute_json.clear();
	coinline::audio_route::snapshot_route_string(m_st, m_prev_route_str);
	coinline::audio_route::snapshot_mute_json(m_st, m_prev_mute_json);
}

std::uint64_t millennium_audio_route_device::machine_cycle_approx() const
{
	if (auto *z = subdevice<z180_device>("maincpu"))
		return z->total_cycles();
	return 0ULL;
}

void millennium_audio_route_device::emit_route_change(std::uint64_t cycle, std::uint16_t pc, char const *reason)
{
	std::string cur_route, cur_mute;
	coinline::audio_route::snapshot_route_string(m_st, cur_route);
	coinline::audio_route::snapshot_mute_json(m_st, cur_mute);
	if (cur_route == m_prev_route_str)
		return;
	double const sec = machine().time().as_double();
	auto const ns = static_cast<std::uint64_t>(sec * 1e9);
	std::string call_state;
	coinline::audio_route::snapshot_integration_call_state(m_st, call_state);
	char const *const cs = call_state.empty() ? nullptr : call_state.c_str();
	std::string const line = millennium_audio_trace_route_change_json(ns, cycle, pc, m_prev_route_str, cur_route,
		m_prev_mute_json, cur_mute, reason, cs);
	millennium_audio_trace_emit_audio_route_path_row(line);
	m_prev_route_str = std::move(cur_route);
	m_prev_mute_json = std::move(cur_mute);
}

void millennium_audio_route_device::emit_mute_change(std::uint64_t cycle, std::uint16_t pc)
{
	std::string cur_route, cur_mute;
	coinline::audio_route::snapshot_route_string(m_st, cur_route);
	coinline::audio_route::snapshot_mute_json(m_st, cur_mute);
	if (cur_mute == m_prev_mute_json)
		return;
	double const sec = machine().time().as_double();
	auto const ns = static_cast<std::uint64_t>(sec * 1e9);
	std::string call_state;
	coinline::audio_route::snapshot_integration_call_state(m_st, call_state);
	char const *const cs = call_state.empty() ? nullptr : call_state.c_str();
	std::string const line =
		millennium_audio_trace_mute_change_json(ns, cycle, pc, m_prev_mute_json, cur_mute, cur_route, cs);
	millennium_audio_trace_emit_mute_route_path_row(line);
	m_prev_mute_json = std::move(cur_mute);
	m_prev_route_str = std::move(cur_route);
}

void millennium_audio_route_device::maybe_emit_route_conflict(std::uint64_t cycle, std::uint16_t pc)
{
	if (!m_voice_active)
		return;
	if (!coinline::audio_route::call_state_established(m_st.call))
		return;
	// Voiceware vs line arbitration — profile not fixed; log compatibility row when both voice and estab call.
	double const sec = machine().time().as_double();
	auto const ns = static_cast<std::uint64_t>(sec * 1e9);
	std::string route_now;
	coinline::audio_route::snapshot_route_string(m_st, route_now);
	std::string const line = millennium_audio_trace_route_conflict_json(ns, cycle, pc, route_now.c_str(),
		"compatibility_validation_required:voice_vs_line_priority");
	millennium_audio_trace_emit_audio_route_path_row(line);
}

void millennium_audio_route_device::run_voice_prompt_conditioning(bool voice_now)
{
	char const *seq_on[] = { "handset_rx_mute", "sidetone_suppression_off", "handset_rx_unmute" };
	char const *seq_off[] = { "handset_rx_mute", "sidetone_suppression_on", "handset_rx_unmute" };
	char const *const *seq = voice_now ? seq_on : seq_off;
	std::uint64_t const cy = machine_cycle_approx();
	for (unsigned i = 0; i < 3U; ++i) {
		std::string notes;
		coinline::audio_route::apply_effect(m_st, seq[i], &notes);
		emit_mute_change(cy, 0U);
		emit_route_change(cy, 0U, "voice_prompt_conditioning");
	}
}

void millennium_audio_route_device::notify_voice_active(bool active) noexcept
{
	bool const prev = m_voice_active;
	m_voice_active = active;
	m_st.voice_prompt_path = active;
	std::uint64_t const cy = machine_cycle_approx();
	std::uint16_t const pc = 0U;
	emit_route_change(cy, pc, "notify_voice_active");
	if (active != prev && m_st.modem_carrier_up && coinline::audio_route::call_state_established(m_st.call)) {
		run_voice_prompt_conditioning(active);
	}
	maybe_emit_route_conflict(cy, pc);
}

void millennium_audio_route_device::notify_hook_off(bool off_hook) noexcept
{
	m_st.hook_off = off_hook;
	std::uint64_t const cy = machine_cycle_approx();
	emit_route_change(cy, 0U, "hook");
}

void millennium_audio_route_device::notify_modem_carrier(bool carrier_up) noexcept
{
	bool const had_estab = coinline::audio_route::call_state_established(m_st.call);
	m_st.modem_carrier_up = carrier_up;
	if (!carrier_up && had_estab) {
		// Carrier loss: return toward idle call semantics (no supervision device in this tranche).
		m_st.call = coinline::audio_route::call_state::CALL_IDLE;
		std::string notes = "compatibility_validation_required:carrier_loss_to_idle";
		(void)notes;
	}
	std::uint64_t const cy = machine_cycle_approx();
	emit_route_change(cy, 0U, "modem_carrier");
}

void millennium_audio_route_device::apply_telephony_effect(char const *effect, std::uint8_t raw_byte,
	std::uint64_t cycle, std::uint16_t pc, char const *command_label)
{
	(void)command_label;
	std::string notes;
	bool const ok = coinline::audio_route::apply_effect(m_st, effect, &notes);
	if (!ok && notes.empty())
		notes.assign("compatibility_validation_required:unhandled_effect");
	emit_mute_change(cycle, pc);
	emit_route_change(cycle, pc, ok ? "telephony_command" : "telephony_command_partial");
	(void)raw_byte;
}
