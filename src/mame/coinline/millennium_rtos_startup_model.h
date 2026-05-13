// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>

namespace coinline::rtos {

constexpr std::size_t max_tasks = 16U;

struct task_desc {
	std::uint8_t status = 0;
	std::uint8_t priority = 0;
	std::uint16_t signals = 0;
	std::uint16_t signals_upper = 0;
};

class startup_scheduler_model {
public:
	enum init_signal : std::uint16_t {
		inis_got_tel_inf = 0x0001,
		inis_tel_tmo = 0x0002,
		inis_mem_checked = 0x0004,
		inis_vfd_init = 0x0008,
		inis_got_date_time = 0x0010,
	};

	void reset() noexcept;
	void load_rom_task(std::size_t idx, task_desc const &d) noexcept;
	void copy_rom_to_runtime() noexcept;
	void post_init_signal(std::uint16_t sig) noexcept;
	bool can_dispatch_non_init_tasks() const noexcept;

	bool dispatch_ready() const noexcept { return m_dispatch_ready; }
	bool xflag_system_running() const noexcept { return m_xflag_system_running; }
	bool xflag_all_init_done() const noexcept { return m_xflag_all_init_done; }
	bool alarm_tel_not_responding() const noexcept { return m_alarm_tel_not_responding; }
	bool init_task_present() const noexcept { return m_runtime_count > 0U; }
	std::size_t runtime_count() const noexcept { return m_runtime_count; }

private:
	void recompute_flags() noexcept;

	std::array<task_desc, max_tasks> m_rom_tasks{};
	std::array<task_desc, max_tasks> m_runtime_tasks{};
	std::array<bool, max_tasks> m_rom_task_loaded{};
	std::size_t m_runtime_count = 0U;
	bool m_dispatch_ready = false;
	std::uint16_t m_init_seen = 0U;
	bool m_xflag_system_running = false;
	bool m_xflag_all_init_done = false;
	bool m_alarm_tel_not_responding = false;
};

} // namespace coinline::rtos

