// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_model.h"

#include <cmath>
#include <iostream>

int main()
{
	millennium_audio_model m;
	m.configure({});
	m.reset();
	m.write_tone_select(0);
	m.write_volume(255);
	m.set_handset_loopback(false);
	m.inject_handset_sample(1.f);
	float const y0 = std::fabs(m.output_sample(0.05));
	m.set_handset_loopback(true);
	m.inject_handset_sample(1.f);
	float const y1 = std::fabs(m.output_sample(0.05));
	if (y0 > 0.05f) {
		std::cerr << "expected near silence without loopback\n";
		return 1;
	}
	if (y1 < 0.2f) {
		std::cerr << "expected handset energy in loopback path\n";
		return 1;
	}
	return 0;
}
