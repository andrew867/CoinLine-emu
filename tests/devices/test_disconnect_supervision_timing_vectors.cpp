// SPDX-License-Identifier: GPL-2.0-or-later
// Clock-controlled vectors aligned to terminal_05_disconnect_supervision_emulator_spec.yaml.

#include "millennium_supervision_fsm.h"

#include <iostream>

int main()
{
	using namespace coinline::supervision;

	disconnect_supervision_fsm fsm;
	fsm.reset();
	fsm.configure_timing_windows({ .cutoff_ticks = 5U, .wink_ticks = 2U, .defer_ticks = 4U });
	fsm.arm_disconnect_timer();

	bool saw_wink = false;
	bool saw_defer = false;
	bool saw_timeout_disconnect = false;

	for (unsigned t = 1; t <= 5U; ++t) {
		auto events = fsm.step_clock_tick(t, 0x5000U);
		for (auto const &e : events) {
			if (e.kind == supervision_fsm_event::kind::supervision_status_code && e.status_code == 0x8cU)
				saw_wink = true;
			if (e.kind == supervision_fsm_event::kind::supervision_status_code && e.status_code == 0x8eU)
				saw_defer = true;
			if (e.kind == supervision_fsm_event::kind::disconnect_event && e.disconnect == disconnect_kind::timeout)
				saw_timeout_disconnect = true;
		}
	}

	if (!saw_wink || !saw_defer || !saw_timeout_disconnect) {
		std::cerr << "terminal_05 timing windows vector failed\n";
		return 1;
	}

	std::cout << "disconnect_supervision_timing_vectors ok\n";
	return 0;
}
