// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_rtos_startup_model.h"

namespace coinline::rtos {

void startup_scheduler_model::reset() noexcept
{
	m_rom_tasks.fill(task_desc{});
	m_runtime_tasks.fill(task_desc{});
	m_rom_task_loaded.fill(false);
	m_runtime_count = 0U;
	m_dispatch_ready = false;
	m_init_seen = 0U;
	m_xflag_system_running = false;
	m_xflag_all_init_done = false;
	m_alarm_tel_not_responding = false;
}

void startup_scheduler_model::load_rom_task(std::size_t idx, task_desc const &d) noexcept
{
	if (idx >= max_tasks)
		return;
	m_rom_tasks[idx] = d;
	m_rom_task_loaded[idx] = true;
}

void startup_scheduler_model::copy_rom_to_runtime() noexcept
{
	m_runtime_tasks.fill(task_desc{});
	m_runtime_count = 0U;
	for (std::size_t i = 0; i < max_tasks; ++i) {
		if (!m_rom_task_loaded[i])
			continue;
		m_runtime_tasks[m_runtime_count++] = m_rom_tasks[i];
	}
	m_dispatch_ready = true;
}

void startup_scheduler_model::post_init_signal(std::uint16_t sig) noexcept
{
	m_init_seen = static_cast<std::uint16_t>(m_init_seen | sig);
	recompute_flags();
}

void startup_scheduler_model::recompute_flags() noexcept
{
	if ((m_init_seen & inis_tel_tmo) != 0U)
		m_alarm_tel_not_responding = true;
	if ((m_init_seen & inis_mem_checked) != 0U && (m_init_seen & inis_vfd_init) != 0U)
		m_xflag_system_running = true;

	std::uint16_t const nominal = static_cast<std::uint16_t>(
		inis_got_tel_inf | inis_mem_checked | inis_vfd_init | inis_got_date_time);
	std::uint16_t const degraded = static_cast<std::uint16_t>(
		inis_tel_tmo | inis_mem_checked | inis_vfd_init | inis_got_date_time);
	if ((m_init_seen & nominal) == nominal || (m_init_seen & degraded) == degraded)
		m_xflag_all_init_done = true;
}

bool startup_scheduler_model::can_dispatch_non_init_tasks() const noexcept
{
	return m_dispatch_ready && m_runtime_count > 0U && m_xflag_all_init_done;
}

} // namespace coinline::rtos

