// SPDX-License-Identifier: GPL-2.0-or-later
// Vector checks aligned to terminal_23_user_io_feature_overlay_profiles_spec.yaml.

#include "millennium_user_io_overlay_model.h"

#include <iostream>

int main()
{
	using namespace coinline::userio;

	{
		overlay_chain_state s{};
		s.overlay_enabled = true;
		s.quick_key_enabled = true;
		if (arbitrate_ordered_consumer(s, 0xA3U) != consumer::overlay_profile) {
			std::cerr << "overlay must win dispatch chain\n";
			return 1;
		}
	}

	{
		overlay_chain_state s{};
		s.overlay_enabled = false;
		s.quick_key_enabled = true;
		if (arbitrate_ordered_consumer(s, 0xA3U) != consumer::quick_key) {
			std::cerr << "quick-key fallback order failed\n";
			return 2;
		}
	}

	{
		overlay_chain_state s{};
		s.overlay_enabled = false;
		s.quick_key_enabled = false;
		if (arbitrate_ordered_consumer(s, 0x05U) != consumer::numeric_keypad) {
			std::cerr << "numeric fallback order failed\n";
			return 3;
		}
	}

	{
		overlay_chain_state s{};
		s.overlay_enabled = false;
		s.quick_key_enabled = false;
		s.numeric_enabled = false;
		if (arbitrate_ordered_consumer(s, 0xE1U) != consumer::system_fallback) {
			std::cerr << "system fallback order failed\n";
			return 4;
		}
	}

	std::cout << "user_io_overlay_arbitration_vectors ok\n";
	return 0;
}
