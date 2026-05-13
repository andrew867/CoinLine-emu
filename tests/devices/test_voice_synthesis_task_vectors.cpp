// SPDX-License-Identifier: GPL-2.0-or-later
// Vector checks aligned to terminal_14_voice_synthesis_emulator_spec.yaml.

#include "millennium_voice_synthesis_model.h"

#include <iostream>

int main()
{
	using namespace coinline::voice;

	{
		synthesis_task_model m;
		m.reset();
		m.start_phrase(0x33U, 100U, 150U);
		m.mark_phrase_completed(130U);
		if (m.task_state() != synthesis_task_model::state::completed || !m.signal_completed() || m.signal_timeout()
			|| m.alarm_not_responding()) {
			std::cerr << "voice synthesis completion signal vector failed\n";
			return 1;
		}
	}

	{
		synthesis_task_model m;
		m.reset();
		m.start_phrase(0x44U, 200U, 240U);
		m.tick(241U);
		if (m.task_state() != synthesis_task_model::state::timeout || !m.signal_timeout()
			|| !m.alarm_not_responding()) {
			std::cerr << "voice synthesis timeout vector failed\n";
			return 2;
		}
	}

	{
		synthesis_task_model m;
		m.reset();
		m.start_phrase(0x55U, 300U, 340U);
		m.mark_phrase_completed(350U);
		if (m.task_state() != synthesis_task_model::state::timeout || !m.signal_timeout()
			|| m.signal_completed()) {
			std::cerr << "voice synthesis late completion vector failed\n";
			return 3;
		}
	}

	std::cout << "voice_synthesis_task_vectors ok\n";
	return 0;
}
