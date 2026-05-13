// SPDX-License-Identifier: GPL-2.0-or-later
// IIR model: RDA pending (0x04) when IER bit0 RX and queue non-empty (mirrors emulator recompute_iir path).

#include <cstdint>
#include <deque>
#include <iostream>

static std::uint8_t model_iir(bool ier_rx_enabled, std::deque<std::uint8_t> const &rxq)
{
	if (ier_rx_enabled && !rxq.empty())
		return 0x04U;
	return 0x01U;
}

int main()
{
	std::deque<std::uint8_t> q;
	if (model_iir(false, q) != 0x01U)
		return 1;
	if (model_iir(true, q) != 0x01U)
		return 2;
	q.push_back(0x72U);
	if (model_iir(true, q) != 0x04U)
		return 3;
	q.pop_front();
	if (model_iir(true, q) != 0x01U)
		return 4;
	std::cout << "external_uart_interrupts ok\n";
	return 0;
}
