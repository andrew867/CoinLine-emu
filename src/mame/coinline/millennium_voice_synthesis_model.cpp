// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voice_synthesis_model.h"

namespace coinline::voice {

void synthesis_task_model::reset() noexcept
{
	m_state = state::idle;
	m_phrase_id = 0U;
	m_start_cycle = 0ULL;
	m_timeout_cycle = 0ULL;
	m_signal_completed = false;
	m_signal_timeout = false;
	m_alarm_not_responding = false;
}

void synthesis_task_model::start_phrase(std::uint8_t phrase_id, std::uint64_t start_cycle,
	std::uint64_t timeout_cycle) noexcept
{
	m_state = state::playing;
	m_phrase_id = phrase_id;
	m_start_cycle = start_cycle;
	m_timeout_cycle = timeout_cycle;
	m_signal_completed = false;
	m_signal_timeout = false;
	m_alarm_not_responding = false;
}

void synthesis_task_model::mark_phrase_completed(std::uint64_t cycle) noexcept
{
	if (m_state != state::playing)
		return;
	if (cycle > m_timeout_cycle) {
		m_state = state::timeout;
		m_signal_timeout = true;
		m_alarm_not_responding = true;
		return;
	}
	m_state = state::completed;
	m_signal_completed = true;
}

void synthesis_task_model::tick(std::uint64_t cycle) noexcept
{
	if (m_state != state::playing)
		return;
	if (cycle <= m_timeout_cycle)
		return;
	m_state = state::timeout;
	m_signal_timeout = true;
	m_alarm_not_responding = true;
}

} // namespace coinline::voice
