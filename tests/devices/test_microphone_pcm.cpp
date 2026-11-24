// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_mic_pcm.h"

#include <cmath>
#include <iostream>
#include <numbers>

namespace {

double const cycle = 2.0 * std::numbers::pi;

bool check_tone(double freq, double min_gain, double max_gain)
{
	millennium_mic_pcm mic;
	std::array<std::int16_t, 480> input;
	unsigned count = 0;
	unsigned const width = 80;
	double sin_sum = 0.;
	double cos_sum = 0.;
	double const output_freq = freq > 4000. ? 8000. - freq : freq;
	for (unsigned block = 0; block < 96; ++block) {
		for (unsigned i = 0; i < input.size(); ++i)
			input[i] = static_cast<std::int16_t>(16000. * std::sin(cycle * freq * (block * input.size() + i) / 48000.));
		mic.push(input.data(), input.size());
		for (unsigned i = 0; i < width; ++i) {
			float const sample = mic.read();
			if (block < 6)
				continue;
			double const angle = cycle * output_freq * count++ / 8000.;
			sin_sum += sample * std::sin(angle);
			cos_sum += sample * std::cos(angle);
		}
	}
	double const gain = 2. * std::hypot(sin_sum, cos_sum) / count / (16000. / 32768.);
	if (gain < min_gain || gain > max_gain) {
		std::cerr << freq << " Hz input had gain " << gain << '\n';
		return false;
	}
	for (unsigned i = 0; i < 1000; ++i)
		mic.read();
	return mic.read() == 0.f;
}

} // namespace

int main()
{
	if (!check_tone(700., 0.95, 1.05) || !check_tone(7000., 0., 0.02)) {
		std::cerr << "expected voice-band PCM, suppression of high-frequency aliases, and silence on underrun\n";
		return 1;
	}

	millennium_mic_pcm mic;
	std::array<std::int16_t, 8192> input;
	std::fill(input.begin(), input.begin() + 6200, -16000);
	std::fill(input.begin() + 6200, input.end(), 16000);
	mic.push(input.data(), input.size());
	if (mic.read() < 0.45f) {
		std::cerr << "expected recent microphone samples after falling behind\n";
		return 1;
	}
	return 0;
}
