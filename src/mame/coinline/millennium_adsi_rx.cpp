// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_adsi_rx.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double tau = 6.2831853071795864769;
constexpr double rate = 8000.0;
}

millennium_adsi_rx::fit::fit(double frequency)
{
	for (unsigned i = 0; i < c.size(); ++i) {
		c[i] = std::cos(tau * frequency * i / rate);
		s[i] = std::sin(tau * frequency * i / rate);
		cc += c[i] * c[i]; ss += s[i] * s[i]; cs += c[i] * s[i];
	}
	determinant = cc * ss - cs * cs;
}

double millennium_adsi_rx::fit::power(std::array<double, 7> const &window) const noexcept
{
	double a = 0, b = 0;
	for (unsigned i = 0; i < c.size(); ++i) {
		a += window[i] * c[i]; b += window[i] * s[i];
	}
	return (ss * a * a - 2 * cs * a * b + cc * b * b) / determinant;
}

millennium_adsi_rx::millennium_adsi_rx() : m_mark(1200), m_space(2200) { reset(); }

void millennium_adsi_rx::clear_fsk() noexcept
{
	m_fsk_count = m_mark_run = m_no_signal = 0;
	m_carrier = false;
	m_symbol = -1;
	m_byte = 0;
	m_next_symbol = 0;
	m_bytes.clear();
}

void millennium_adsi_rx::reset() noexcept
{
	m_cas_count = 0;
	m_cas = false;
	m_samples = 0;
	m_fsk_enabled = false;
	m_framing_errors = 0;
	clear_fsk();
}

void millennium_adsi_rx::finish_cas_window() noexcept
{
	double energy = 0;
	for (double v : m_cas_window) energy += v * v;
	auto tone_power = [&](double coefficient) {
		double q1 = 0, q2 = 0;
		for (double v : m_cas_window) {
			double const q0 = v + coefficient * q1 - q2;
			q2 = q1; q1 = q0;
		}
		return 2 * (q1 * q1 + q2 * q2 - coefficient * q1 * q2) / double(m_cas_window.size());
	};
	static double const low_coefficient = 2 * std::cos(tau * 2130 / rate);
	static double const high_coefficient = 2 * std::cos(tau * 2750 / rate);
	double const low = tone_power(low_coefficient), high = tone_power(high_coefficient);
	m_cas = energy > 80 * 200.0 * 200.0 && low > energy * 0.18 && high > energy * 0.18
		&& low + high > energy * 0.65 && low < high * 4.0 && high < low * 4.0;
}

void millennium_adsi_rx::sample(std::int16_t input, bool fsk_enabled)
{
	m_cas_window[m_cas_count++] = input;
	if (m_cas_count == m_cas_window.size()) {
		finish_cas_window();
		m_cas_count = 0;
	}
	std::uint64_t const tick = ++m_samples * 3U;
	if (!fsk_enabled) {
		if (m_fsk_enabled) clear_fsk();
		m_fsk_enabled = false;
		return;
	}
	m_fsk_enabled = true;
	if (m_fsk_count < m_fsk_window.size())
		m_fsk_window[m_fsk_count++] = input;
	else {
		std::move(m_fsk_window.begin() + 1, m_fsk_window.end(), m_fsk_window.begin());
		m_fsk_window.back() = input;
	}
	if (m_fsk_count != m_fsk_window.size()) return;
	double energy = 0;
	for (double v : m_fsk_window) energy += v * v;
	double const mark = m_mark.power(m_fsk_window), space = m_space.power(m_fsk_window);
	// A window spanning a frequency transition need not fit either tone well.
	// Preserve the symbol timing through those windows; require a clean mark
	// for carrier acquisition, and use energy to detect carrier loss.
	bool const signal = energy > 7 * 200.0 * 200.0;
	bool const bit = mark > space;
	unsigned const prior_mark = m_mark_run;
	// Start-of-carrier requires a sustained coherent 1200 Hz mark, not just
	// a succession of windows that happen to favour the lower frequency.
	bool const coherent_mark = signal && mark > energy * 0.65 && mark > space * 1.7;
	m_mark_run = (m_carrier ? signal && bit : coherent_mark) ? std::min(m_mark_run + 1U, 1000U) : 0;
	if (m_mark_run >= 100 && coherent_mark) m_carrier = true;
	if (signal) m_no_signal = 0;
	else if (++m_no_signal > 40) {
		m_carrier = false;
		m_symbol = -1;
		return;
	}
	if (m_symbol < 0) {
		if (m_carrier && signal && !bit && prior_mark >= 3) {
			m_symbol = 0;
			m_byte = 0;
			// 20 ticks per symbol at 8 kHz, using thirds of a sample.
			// The seven-sample discriminator has a short group delay.
			m_next_symbol = tick + 7;
		}
		return;
	}
	if (tick < m_next_symbol) return;
	if (m_symbol == 0 && bit) {
		m_symbol = -1;
		return;
	}
	if (m_symbol >= 1 && m_symbol <= 8 && bit)
		m_byte |= std::uint8_t(1U << (m_symbol - 1));
	if (m_symbol == 9) {
		if (bit && m_bytes.size() < 512) m_bytes.push_back(m_byte);
		else ++m_framing_errors;
		m_symbol = -1;
		return;
	}
	++m_symbol;
	m_next_symbol += 20;
}

bool millennium_adsi_rx::take_byte(std::uint8_t &value)
{
	if (m_bytes.empty()) return false;
	value = m_bytes.front(); m_bytes.pop_front();
	return true;
}
