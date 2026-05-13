// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_route_apply.h"

#include <cstring>
#include <iostream>

int main()
{
	char const *lb = nullptr;
	char const *ef = nullptr;
	if (!coinline::audio_route::lookup_command(0x27, lb, ef))
		return 1;
	if (std::strcmp(lb, "TX_MUTE") != 0 || std::strcmp(ef, "handset_tx_mute") != 0) {
		std::cerr << "lookup 0x27 mismatch\n";
		return 1;
	}
	if (!coinline::audio_route::lookup_command(0x41, lb, ef))
		return 1;
	if (std::strcmp(ef, "call_state_dialing") != 0)
		return 1;
	if (coinline::audio_route::lookup_command(0x03, lb, ef))
		return 1;
	std::cout << "telephony decode table ok\n";
	return 0;
}
