// SPDX-License-Identifier: GPL-2.0-or-later
// Milestone semantics: M7B evidence is CSIO-modeled parser success, not UART alias alone.

#include <cstdint>
#include <iostream>

static bool csio_status_consumed(std::uint8_t const *frame, unsigned n)
{
	std::uint8_t code = 0, len = 0, sum = 0;
	unsigned rem = 0;
	bool inf = false;
	for (unsigned i = 0; i < n; i++) {
		std::uint8_t const b = frame[i];
		if (!inf) {
			if (b < 0xc0U)
				continue;
			inf = true;
			code = b;
			sum = b;
			len = 0;
			rem = 0;
			continue;
		}
		if (len == 0) {
			len = b;
			sum = static_cast<std::uint8_t>(sum + b);
			if (len < 3U) {
				inf = false;
				continue;
			}
			rem = len - 2U;
			continue;
		}
		if (rem == 0U) {
			inf = false;
			continue;
		}
		rem--;
		if (rem == 0U) {
			bool const ok = (sum == b);
			inf = false;
			if (ok && code == 0xc0U)
				return true;
		} else {
			sum = static_cast<std::uint8_t>(sum + b);
		}
	}
	return false;
}

int main()
{
	std::uint8_t uart_only[] = { 0x72U };
	if (csio_status_consumed(uart_only, sizeof uart_only)) {
		std::cerr << "ACK alone must not imply M7B\n";
		return 1;
	}
	std::uint8_t st[8];
	st[0] = 0xc0U;
	st[1] = 0x08U;
	st[7] = 0;
	st[7] = static_cast<std::uint8_t>(st[0] + st[1]);
	for (int i = 2; i < 7; i++)
		st[i] = 0;
	if (!csio_status_consumed(st, 8)) {
		std::cerr << "idle status frame must validate\n";
		return 2;
	}
	std::cout << "telephony_ready_milestone ok\n";
	return 0;
}
