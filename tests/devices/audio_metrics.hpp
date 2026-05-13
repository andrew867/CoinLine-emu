// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

inline double constexpr pi()
{
	return 3.14159265358979323846;
}

inline double tone_energy_at(float const *x, int n, double sample_rate_hz, double freq_hz)
{
	double s = 0.0;
	double c = 0.0;
	for (int i = 0; i < n; ++i) {
		double const ang = 2.0 * pi() * freq_hz * double(i) / sample_rate_hz;
		s += double(x[i]) * std::sin(ang);
		c += double(x[i]) * std::cos(ang);
	}
	double const mag = std::sqrt(s * s + c * c) / double(n > 0 ? n : 1);
	return mag;
}

inline float rms_window(float const *x, int n)
{
	double t = 0.0;
	for (int i = 0; i < n; ++i)
		t += double(x[i]) * double(x[i]);
	return static_cast<float>(std::sqrt(t / double(n > 0 ? n : 1)));
}
