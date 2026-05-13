// SPDX-License-Identifier: GPL-2.0-or-later
// Mirrors CSIO-side variable-length telephony framing parser in millennium_state::telephony_note_csio_rx_byte.

#include <cstdint>
#include <iostream>

struct csio_rx_model {
	bool in_frame = false;
	std::uint8_t code = 0;
	std::uint8_t len = 0;
	std::uint8_t sum = 0;
	unsigned remaining = 0;
	bool status_ok = false;
	bool error_ok = false;

	void feed(std::uint8_t v)
	{
		auto on_var = [&](std::uint8_t b) {
			if (!in_frame) {
				if (b >= 0xc0U) {
					in_frame = true;
					code = b;
					sum = b;
					len = 0;
					remaining = 0;
				}
				return;
			}
			if (len == 0) {
				len = b;
				sum = static_cast<std::uint8_t>(sum + b);
				if (len < 3U) {
					in_frame = false;
					return;
				}
				remaining = unsigned(len - 2U);
				return;
			}
			if (remaining == 0U) {
				in_frame = false;
				return;
			}
			remaining--;
			if (remaining == 0U) {
				bool const ok = (sum == b);
				if (ok && code == 0xc0U)
					status_ok = true;
				else if (ok && code == 0xc4U)
					error_ok = true;
				in_frame = false;
			} else {
				sum = static_cast<std::uint8_t>(sum + b);
			}
		};
		on_var(v);
	}
};

static void build_status_idle(std::uint8_t *out8)
{
	std::uint8_t const code = 0xc0U;
	std::uint8_t const len = 0x08U;
	std::uint8_t sum = static_cast<std::uint8_t>(code + len);
	out8[0] = code;
	out8[1] = len;
	for (int i = 0; i < 5; i++) {
		out8[2 + i] = 0;
		sum = static_cast<std::uint8_t>(sum + out8[2 + i]);
	}
	out8[7] = sum;
}

int main()
{
	std::uint8_t frame[8];
	build_status_idle(frame);
	csio_rx_model m;
	for (std::uint8_t b : frame)
		m.feed(b);
	if (!m.status_ok) {
		std::cerr << "status frame not accepted\n";
		return 1;
	}
	csio_rx_model hook;
	hook.feed(0x6cU);
	if (hook.in_frame || hook.status_ok) {
		std::cerr << "single byte should not open var frame\n";
		return 2;
	}
	std::cout << "telephony_rx_path ok\n";
	return 0;
}
