// SPDX-License-Identifier: GPL-2.0-or-later
// Class A: integration call-state labels from composite hardware state only (no scenario injection).

#include "millennium_audio_route_apply.h"

#include <iostream>
#include <string>

int main()
{
	coinline::audio_route::composite_state st{};
	coinline::audio_route::reset_to_idle_fixture(st);
	std::string cs;
	coinline::audio_route::snapshot_integration_call_state(st, cs);
	if (cs != "idle_on_hook") {
		std::cerr << "expected idle_on_hook got " << cs << '\n';
		return 1;
	}

	st.voice_prompt_path = true;
	coinline::audio_route::snapshot_integration_call_state(st, cs);
	if (cs != "prompt_playback") {
		std::cerr << "expected prompt_playback got " << cs << '\n';
		return 1;
	}

	st.voice_prompt_path = false;
	st.call = coinline::audio_route::call_state::CALL_DIALING;
	coinline::audio_route::snapshot_integration_call_state(st, cs);
	if (cs != "dialing") {
		std::cerr << "expected dialing got " << cs << '\n';
		return 1;
	}

	st.call = coinline::audio_route::call_state::CALL_ESTAB_SUPVSED;
	st.modem_carrier_up = true;
	coinline::audio_route::snapshot_integration_call_state(st, cs);
	if (cs != "connected_call") {
		std::cerr << "expected connected_call got " << cs << '\n';
		return 1;
	}

	st.modem_carrier_up = false;
	coinline::audio_route::snapshot_integration_call_state(st, cs);
	if (cs != "disconnect_detected") {
		std::cerr << "expected disconnect_detected got " << cs << '\n';
		return 1;
	}

	st.call = coinline::audio_route::call_state::CALL_IDLE;
	st.modem_carrier_up = false;
	st.hook_off = true;
	coinline::audio_route::snapshot_integration_call_state(st, cs);
	if (cs != "off_hook") {
		std::cerr << "expected off_hook got " << cs << '\n';
		return 1;
	}

	return 0;
}
