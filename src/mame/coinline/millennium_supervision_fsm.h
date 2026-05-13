// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <vector>

namespace coinline::supervision {

enum class disconnect_kind {
	none,
	normal_disconnect,
	cpc,
	timeout,
	fault,
};

/// Single emitted supervision-class trace event for replay/tests (no MAME).
struct supervision_fsm_event {
	enum class kind : std::uint8_t { supervision_status_code, disconnect_event, supervision_timeout } kind;

	std::uint8_t status_code = 0; // when kind == supervision_status_code
	disconnect_kind disconnect = disconnect_kind::none; // when kind == disconnect_event

	std::uint64_t cycle = 0;
	std::uint16_t pc = 0;
};

/// Finite abstraction for disconnect supervision — fused carrier + processor→host codes per
/// `fixtures/board/disconnect-supervision-map.json`.
class disconnect_supervision_fsm {
public:
	struct timing_windows {
		unsigned cutoff_ticks = 0U;
		unsigned wink_ticks = 0U;
		unsigned defer_ticks = 0U;
	};

	void reset();
	void configure_timing_windows(timing_windows const &w) noexcept;
	void arm_disconnect_timer() noexcept;

	std::vector<supervision_fsm_event> step_carrier(bool carrier_up, std::uint64_t cycle, std::uint16_t pc);
	std::vector<supervision_fsm_event> step_status(std::uint8_t code, std::uint64_t cycle, std::uint16_t pc);
	std::vector<supervision_fsm_event> step_watchdog_fired(std::uint64_t cycle, std::uint16_t pc);
	std::vector<supervision_fsm_event> step_clock_tick(std::uint64_t cycle, std::uint16_t pc);

private:
	static bool is_profile_status_code(std::uint8_t c) noexcept;

	std::vector<supervision_fsm_event> push_status(std::uint8_t code, std::uint64_t cycle, std::uint16_t pc);

	bool m_carrier = false;
	bool m_line_connected = false;
	bool m_interruption_after_connect = false;
	enum class rev_phase : std::uint8_t { idle, saw_rev0 } m_rev = rev_phase::idle;
	bool m_disconnect_emitted = false;
	timing_windows m_windows{};
	bool m_timer_armed = false;
	unsigned m_timer_ticks = 0U;
	bool m_wink_emitted = false;
	bool m_defer_emitted = false;
};

} // namespace coinline::supervision
