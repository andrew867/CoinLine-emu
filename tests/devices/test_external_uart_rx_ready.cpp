// SPDX-License-Identifier: GPL-2.0-or-later
// Behavioral spec: external 16550 RX queue + LSR DR bit (mirrors millennium_state::external_uart_* LSR shadow rule).

#include <cstdint>
#include <deque>
#include <iostream>

static std::uint8_t model_lsr_dr(std::deque<std::uint8_t> const &q)
{
	return static_cast<std::uint8_t>(0x60U | (q.empty() ? 0x00U : 0x01U));
}

int main()
{
	std::deque<std::uint8_t> q;
	if (model_lsr_dr(q) != 0x60U) {
		std::cerr << "lsr empty mismatch\n";
		return 1;
	}
	q.push_back(0x55U);
	if (model_lsr_dr(q) != 0x61U) {
		std::cerr << "lsr pending mismatch\n";
		return 2;
	}
	std::uint8_t const v = q.front();
	q.pop_front();
	if (v != 0x55U || !q.empty()) {
		std::cerr << "pop mismatch\n";
		return 3;
	}
	if (model_lsr_dr(q) != 0x60U) {
		std::cerr << "lsr clear mismatch\n";
		return 4;
	}
	std::cout << "external_uart_rx_ready ok\n";
	return 0;
}
