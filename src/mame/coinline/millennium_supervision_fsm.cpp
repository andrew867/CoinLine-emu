// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_supervision_fsm.h"

namespace coinline::supervision {

namespace {

supervision_fsm_event make_status(std::uint8_t code, std::uint64_t cycle, std::uint16_t pc)
{
	supervision_fsm_event e{};
	e.kind = supervision_fsm_event::kind::supervision_status_code;
	e.status_code = code;
	e.cycle = cycle;
	e.pc = pc;
	return e;
}

supervision_fsm_event make_disconnect(disconnect_kind k, std::uint64_t cycle, std::uint16_t pc)
{
	supervision_fsm_event e{};
	e.kind = supervision_fsm_event::kind::disconnect_event;
	e.disconnect = k;
	e.cycle = cycle;
	e.pc = pc;
	return e;
}

supervision_fsm_event make_timeout(std::uint64_t cycle, std::uint16_t pc)
{
	supervision_fsm_event e{};
	e.kind = supervision_fsm_event::kind::supervision_timeout;
	e.cycle = cycle;
	e.pc = pc;
	return e;
}

} // namespace

bool disconnect_supervision_fsm::is_profile_status_code(std::uint8_t c) noexcept
{
	switch (c) {
	case 0x60:
	case 0x62:
	case 0x64:
	case 0x66:
	case 0x68:
	case 0x6a:
	case 0x6c:
	case 0x6e:
	case 0x8c:
	case 0x8e:
		return true;
	default:
		return false;
	}
}

void disconnect_supervision_fsm::reset()
{
	m_carrier = false;
	m_line_connected = false;
	m_interruption_after_connect = false;
	m_rev = rev_phase::idle;
	m_disconnect_emitted = false;
	m_timer_armed = false;
	m_timer_ticks = 0U;
	m_wink_emitted = false;
	m_defer_emitted = false;
}

void disconnect_supervision_fsm::configure_timing_windows(timing_windows const &w) noexcept
{
	m_windows = w;
}

void disconnect_supervision_fsm::arm_disconnect_timer() noexcept
{
	m_timer_armed = true;
	m_timer_ticks = 0U;
	m_wink_emitted = false;
	m_defer_emitted = false;
}

std::vector<supervision_fsm_event> disconnect_supervision_fsm::push_status(std::uint8_t code,
	std::uint64_t cycle, std::uint16_t pc)
{
	std::vector<supervision_fsm_event> out;
	if (!is_profile_status_code(code))
		return out;
	out.push_back(make_status(code, cycle, pc));

	if (code == 0x66U)
		m_line_connected = true;
	if (code == 0x64U && m_line_connected)
		m_interruption_after_connect = true;

	if (code == 0x68U)
		m_rev = rev_phase::saw_rev0;
	else if (code == 0x6AU && m_rev == rev_phase::saw_rev0) {
		m_rev = rev_phase::idle;
		if (!m_disconnect_emitted) {
			out.push_back(make_disconnect(disconnect_kind::cpc, cycle, pc));
			m_disconnect_emitted = true;
		}
	}

	return out;
}

std::vector<supervision_fsm_event> disconnect_supervision_fsm::step_carrier(bool carrier_up,
	std::uint64_t cycle, std::uint16_t pc)
{
	std::vector<supervision_fsm_event> out;
	if (!carrier_up && m_carrier && m_line_connected && m_interruption_after_connect &&
	    !m_disconnect_emitted) {
		out.push_back(make_disconnect(disconnect_kind::normal_disconnect, cycle, pc));
		m_disconnect_emitted = true;
	}
	m_carrier = carrier_up;
	return out;
}

std::vector<supervision_fsm_event> disconnect_supervision_fsm::step_status(std::uint8_t code,
	std::uint64_t cycle, std::uint16_t pc)
{
	return push_status(code, cycle, pc);
}

std::vector<supervision_fsm_event> disconnect_supervision_fsm::step_watchdog_fired(std::uint64_t cycle,
	std::uint16_t pc)
{
	std::vector<supervision_fsm_event> out;
	out.push_back(make_timeout(cycle, pc));
	if (!m_disconnect_emitted) {
		out.push_back(make_disconnect(disconnect_kind::timeout, cycle, pc));
		m_disconnect_emitted = true;
	}
	return out;
}

std::vector<supervision_fsm_event> disconnect_supervision_fsm::step_clock_tick(std::uint64_t cycle,
	std::uint16_t pc)
{
	std::vector<supervision_fsm_event> out;
	if (!m_timer_armed)
		return out;
	m_timer_ticks++;
	if (m_windows.wink_ticks != 0U && !m_wink_emitted && m_timer_ticks >= m_windows.wink_ticks) {
		out.push_back(make_status(0x8cU, cycle, pc));
		m_wink_emitted = true;
	}
	if (m_windows.defer_ticks != 0U && !m_defer_emitted && m_timer_ticks >= m_windows.defer_ticks) {
		out.push_back(make_status(0x8eU, cycle, pc));
		m_defer_emitted = true;
	}
	if (m_windows.cutoff_ticks != 0U && m_timer_ticks >= m_windows.cutoff_ticks && !m_disconnect_emitted) {
		out.push_back(make_timeout(cycle, pc));
		out.push_back(make_disconnect(disconnect_kind::timeout, cycle, pc));
		m_disconnect_emitted = true;
		m_timer_armed = false;
	}
	return out;
}

} // namespace coinline::supervision
