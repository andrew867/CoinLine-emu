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
	m.write_tone_select(2);

	double const sr = 8000.0;
	int const N = int(2.0 * sr);
	std::vector<float> buf(static_cast<std::size_t>(N));
	for (int i = 0; i < N; ++i)
		buf[static_cast<std::size_t>(i)] = m.output_sample(double(i) / sr);

	int const h = int(0.5 * sr);
	float const rms_a = rms_window(buf.data(), h);
	float const rms_b = rms_window(buf.data() + std::size_t(h), h);
	float const hi = rms_a > rms_b ? rms_a : rms_b;
	float const lo = rms_a > rms_b ? rms_b : rms_a;
	if (hi <= lo * 2.f) {
		std::cerr << "busy cadence contrast weak\n";
		return 1;
	}
	return 0;
}
