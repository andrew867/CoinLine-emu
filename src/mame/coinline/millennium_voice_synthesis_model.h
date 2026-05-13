// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

namespace coinline::voice {

class synthesis_task_model {
public:
	enum class state : std::uint8_t {
		idle = 0,
		playing,
		completed,
		timeout,
	};

	void reset() noexcept;
	void start_phrase(std::uint8_t phrase_id, std::uint64_t start_cycle, std::uint64_t timeout_cycle) noexcept;
	void mark_phrase_completed(std::uint64_t cycle) noexcept;
	void tick(std::uint64_t cycle) noexcept;

	state task_state() const noexcept { return m_state; }
	bool signal_completed() const noexcept { return m_signal_completed; }
	bool signal_timeout() const noexcept { return m_signal_timeout; }
	bool alarm_not_responding() const noexcept { return m_alarm_not_responding; }
	std::uint8_t phrase_id() const noexcept { return m_phrase_id; }

private:
	state m_state = state::idle;
	std::uint8_t m_phrase_id = 0U;
	std::uint64_t m_start_cycle = 0ULL;
	std::uint64_t m_timeout_cycle = 0ULL;
	bool m_signal_completed = false;
	bool m_signal_timeout = false;
	bool m_alarm_not_responding = false;
};

} // namespace coinline::voice
