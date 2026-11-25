// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_adsi_rx.h"

#include <cmath>
#include <iostream>
#include <vector>

int main()
{
	constexpr double tau = 6.2831853071795864769;
	millennium_adsi_rx rx;
	for (unsigned i = 0; i < 680; ++i) {
		rx.sample(std::int16_t(2600 * (std::sin(tau * 2130 * i / 8000) +
			std::sin(tau * 2750 * i / 8000))), false);
		if (i >= 79 && !rx.cas_detected()) return 1;
	}
	for (unsigned i = 0; i < 160; ++i) rx.sample(0, false);
	if (rx.cas_detected() || rx.fsk_carrier()) return 2;
	for (unsigned i = 0; i < 680; ++i)
		rx.sample(std::int16_t(5000 * std::sin(tau * 2130 * i / 8000)), false);
	if (rx.cas_detected()) return 3;
	std::vector<std::uint8_t> const message {0x84, 0x05, 0x01, 0x8e, 0, 0x86, 0, 0x62,
		0x00, 0xff, 0x55, 0xaa, 0x81, 0x7e, 0x11, 0x20, 0x04};
	for (unsigned trial = 0; trial < 24; ++trial) {
		rx.reset();
		std::vector<bool> symbols(180, true);
		for (auto byte : message) {
			symbols.push_back(false);
			for (unsigned i = 0; i < 8; ++i) symbols.push_back((byte >> i) & 1);
			symbols.push_back(true);
		}
		symbols.insert(symbols.end(), 12, true);
		double phase = trial * tau / 24;
		unsigned noise = trial + 1;
		for (unsigned i = 0; i * 3 < symbols.size() * 20; ++i) {
			phase += tau * (symbols[(i * 3) / 20] ? 1200 : 2200) / 8000;
			noise = noise * 1664525U + 1013904223U;
			rx.sample(std::int16_t(3800 * std::sin(phase) + int(noise % 501) - 250), true);
		}
		if (!rx.fsk_carrier()) return 4;
		std::vector<std::uint8_t> decoded;
		std::uint8_t byte;
		while (rx.take_byte(byte)) decoded.push_back(byte);
		if (decoded != message || rx.framing_errors()) {
			std::cerr << "Bell 202 decode failure, phase " << trial << ", bytes " << decoded.size() << '\n';
			return 5;
		}
		for (unsigned i = 0; i < 160; ++i) rx.sample(0, true);
		if (rx.fsk_carrier() || rx.take_byte(byte)) return 6;
	}
	rx.reset();
	for (unsigned i = 0; i < 1600; ++i)
		rx.sample(std::int16_t(4000 * std::sin(tau * 1200 * i / 8000)), false);
	std::uint8_t byte;
	if (rx.fsk_carrier() || rx.take_byte(byte)) return 7;
	rx.reset();
	unsigned seed = 12345;
	for (unsigned i = 0; i < 16000; ++i) {
		seed = seed * 1664525U + 1013904223U;
		// Broadband, voice-like interference should not establish a modem mark.
		double const t = double(i) / 8000;
		int const v = int(2600 * std::sin(tau * 330 * t) + 1700 * std::sin(tau * 850 * t)
			+ 1500 * std::sin(tau * 1580 * t) + int(seed % 2001) - 1000);
		rx.sample(std::int16_t(v), true);
		if (rx.fsk_carrier() || rx.take_byte(byte)) return 8;
	}
	return 0;
}
