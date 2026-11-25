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

	m.reset();
	m.inject_handset_sample(0.8f);
	m.set_receive_gain(0.5f);
	if (std::fabs(m.handset_speaker_sample(1.0) - 0.2f) > 0.001f) {
		std::cerr << "expected receive gain to change the handset level\n";
		return 1;
	}
	m.write_dtmf_digit('5', 90U, 1.0);
	float const during_digit = m.handset_speaker_sample(1.015);
	m.inject_handset_sample(0.f);
	if (m.handset_speaker_sample(1.015) != during_digit) {
		std::cerr << "receive audio should be muted while playing keypad DTMF\n";
		return 1;
	}
	m.inject_handset_sample(0.8f);
	m.set_receive_muted(true);
	if (m.handset_speaker_sample(1.2) != 0.f) {
		std::cerr << "expected a muted receive path after the tone\n";
		return 1;
	}
	return 0;
}
