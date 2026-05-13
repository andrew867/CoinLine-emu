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
	m.write_tone_select(0);
	m.write_dtmf_digit('5', 120);

	double const sr = 8000.0;
	int const N = int(0.15 * sr);
	std::vector<float> buf(static_cast<std::size_t>(N));
	for (int i = 0; i < N; ++i)
		buf[static_cast<std::size_t>(i)] = m.output_sample(double(i) / sr);

	double const r770 = tone_energy_at(buf.data(), N, sr, 770.0);
	double const r1336 = tone_energy_at(buf.data(), N, sr, 1336.0);
	double const r300 = tone_energy_at(buf.data(), N, sr, 300.0);
	if (r770 <= r300 * 10.0 || r1336 <= r300 * 10.0) {
		std::cerr << "DTMF digit 5 row/col energy low\n";
		return 1;
	}
	return 0;
}
