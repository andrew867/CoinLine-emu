// SPDX-License-Identifier: GPL-2.0-or-later

#include "audio_metrics.hpp"
#include "millennium_audio_model.h"

#include <iostream>
#include <vector>

int main()
{
	millennium_alerter_board_config cfg{};
	cfg.sample_rate_hz = 8000;
	millennium_audio_model m;
	m.configure(cfg);
	m.reset();
	m.write_volume(255);
	m.write_tone_select(3);

	double const sr = 8000.0;
	int const total_samp = int(6.0 * sr);
	std::vector<float> buf(static_cast<std::size_t>(total_samp));
	for (int i = 0; i < total_samp; ++i) {
		double const t = double(i) / sr;
		buf[static_cast<std::size_t>(i)] = m.output_sample(t);
	}

	float const rms_on = rms_window(buf.data(), int(2.0 * sr));
	float const rms_off = rms_window(buf.data() + std::size_t(2.2 * sr), int(1.0 * sr));
	if (rms_on <= rms_off * 3.f) {
		std::cerr << "ringback on/off contrast weak\n";
		return 1;
	}
	return 0;
}
