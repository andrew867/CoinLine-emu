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
	m.write_tone_select(1);

	double const sr = 8000.0;
	int const N = 8000;
	std::vector<float> buf(static_cast<std::size_t>(N));
	for (int i = 0; i < N; ++i) {
		double const t = double(i) / sr;
		buf[static_cast<std::size_t>(i)] = m.output_sample(t);
	}

	double const e350 = tone_energy_at(buf.data(), N, sr, 350.0);
	double const e440 = tone_energy_at(buf.data(), N, sr, 440.0);
	double const eref = tone_energy_at(buf.data(), N, sr, 123.7);

	if (e350 <= eref * 8.0 || e440 <= eref * 8.0) {
		std::cerr << "dial tone energy weak vs off-frequency bin\n";
		return 1;
	}
	return 0;
}
