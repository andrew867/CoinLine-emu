// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <deque>

// 8 kHz analogue expansion-board receiver. Only samples on the line can
// assert the dual-tone detector or produce a Bell 202 serial byte.
class millennium_adsi_rx {
public:
	millennium_adsi_rx();
	void reset() noexcept;
	void sample(std::int16_t input, bool fsk_enabled);
	bool cas_detected() const noexcept { return m_cas; }
	bool fsk_carrier() const noexcept { return m_carrier; }
	bool take_byte(std::uint8_t &value);
	bool pending_bytes() const noexcept { return !m_bytes.empty(); }
	unsigned framing_errors() const noexcept { return m_framing_errors; }

private:
	struct fit {
		std::array<double, 7> c{}, s{};
		double cc = 0, ss = 0, cs = 0, determinant = 0;
		explicit fit(double frequency);
		double power(std::array<double, 7> const &window) const noexcept;
	};
	void clear_fsk() noexcept;
	void finish_cas_window() noexcept;
	fit m_mark, m_space;
	std::array<double, 80> m_cas_window{};
	unsigned m_cas_count = 0;
	bool m_cas = false;
	std::array<double, 7> m_fsk_window{};
	unsigned m_fsk_count = 0;
	unsigned m_mark_run = 0, m_no_signal = 0;
	bool m_carrier = false, m_fsk_enabled = false;
	int m_symbol = -1;
	std::uint8_t m_byte = 0;
	std::uint64_t m_samples = 0, m_next_symbol = 0;
	unsigned m_framing_errors = 0;
	std::deque<std::uint8_t> m_bytes;
};
