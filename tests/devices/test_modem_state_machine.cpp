// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <iostream>
#include <set>
#include <vector>

int main()
{
	std::vector<millennium_modem_state> declared;
	millennium_modem_model::declared_states(declared);

	millennium_modem_model m;
	std::set<std::string> seen;

	auto touch = [&](millennium_modem_state s) {
		seen.insert(millennium_modem_model::state_cstr(s));
	};

	m.reset();
	touch(m.state());

	struct step {
		char const *event;
		millennium_modem_state expect_after;
	} const steps[] = {
		{"dialing", millennium_modem_state::dialing},
		{"ringing", millennium_modem_state::ringing},
		{"connected", millennium_modem_state::connected},
		{"recover_idle", millennium_modem_state::idle},
		{"busy", millennium_modem_state::busy},
		{"recover_idle", millennium_modem_state::idle},
		{"no_answer", millennium_modem_state::no_answer},
		{"recover_idle", millennium_modem_state::idle},
		{"connected", millennium_modem_state::connected},
		{"carrier_lost", millennium_modem_state::carrier_lost},
		{"recover_idle", millennium_modem_state::idle},
		{"connected", millennium_modem_state::connected},
		{"noisy_line", millennium_modem_state::noisy_line},
	};

	for (auto const &st : steps) {
		if (!m.inject_event(st.event)) {
			std::cerr << "inject failed: " << st.event << "\n";
			return 1;
		}
		if (m.state() != st.expect_after) {
			std::cerr << "state mismatch after " << st.event << "\n";
			return 1;
		}
		touch(m.state());
	}

	for (auto s : declared) {
		char const *const name = millennium_modem_model::state_cstr(s);
		if (!seen.count(name)) {
			std::cerr << "declared state not exercised: " << name << "\n";
			return 1;
		}
	}
	return 0;
}
