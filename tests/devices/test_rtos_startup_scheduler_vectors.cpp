// SPDX-License-Identifier: GPL-2.0-or-later
// Vector checks aligned to terminal_16_rtos_startup_scheduler_spec.yaml.

#include "millennium_rtos_startup_model.h"

#include <cstdint>
#include <iostream>

int main()
{
	using namespace coinline::rtos;

	{
		startup_scheduler_model m;
		m.reset();
		task_desc init_task{};
		init_task.status = 1U;
		init_task.priority = 1U;
		init_task.signals_upper = 0x0001U;
		m.load_rom_task(0U, init_task);
		task_desc other_task{};
		other_task.status = 1U;
		other_task.priority = 2U;
		m.load_rom_task(4U, other_task);
		m.copy_rom_to_runtime();
		if (!m.dispatch_ready() || !m.init_task_present() || m.runtime_count() != 2U) {
			std::cerr << "rom task copy/dispatch vector failed\n";
			return 1;
		}
	}

	{
		startup_scheduler_model m;
		m.reset();
		task_desc init_task{};
		init_task.status = 1U;
		m.load_rom_task(0U, init_task);
		m.copy_rom_to_runtime();
		m.post_init_signal(startup_scheduler_model::inis_got_tel_inf);
		m.post_init_signal(startup_scheduler_model::inis_mem_checked);
		m.post_init_signal(startup_scheduler_model::inis_vfd_init);
		m.post_init_signal(startup_scheduler_model::inis_got_date_time);
		if (!m.xflag_system_running() || !m.xflag_all_init_done() || m.alarm_tel_not_responding()
			|| !m.can_dispatch_non_init_tasks()) {
			std::cerr << "init rendezvous nominal vector failed\n";
			return 2;
		}
	}

	{
		startup_scheduler_model m;
		m.reset();
		task_desc init_task{};
		init_task.status = 1U;
		m.load_rom_task(0U, init_task);
		m.copy_rom_to_runtime();
		m.post_init_signal(startup_scheduler_model::inis_tel_tmo);
		m.post_init_signal(startup_scheduler_model::inis_mem_checked);
		m.post_init_signal(startup_scheduler_model::inis_vfd_init);
		m.post_init_signal(startup_scheduler_model::inis_got_date_time);
		if (!m.alarm_tel_not_responding() || !m.xflag_all_init_done() || !m.can_dispatch_non_init_tasks()) {
			std::cerr << "telephony timeout degraded boot vector failed\n";
			return 3;
		}
	}

	std::cout << "rtos_startup_scheduler_vectors ok\n";
	return 0;
}

