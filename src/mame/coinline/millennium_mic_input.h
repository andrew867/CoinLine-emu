// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_mic_pcm.h"

#include <string_view>

// Host capture may require stereo and a native sample rate other than 48 kHz.
// The handset still sees the existing mono, band-limited 8 kHz PCM path.
class millennium_mic_input {
public:
	void configure(unsigned rate, unsigned channels) noexcept
	{
		m_rate = std::clamp(rate, 8000U, 48000U);
		m_channels = channels == 2 ? 2U : 1U;
		m_phase = 0;
		m_have_previous = false;
	}

	void push(millennium_mic_pcm &pcm, std::int16_t const *input, std::size_t frames) noexcept
	{
		if (m_rate == 48000 && m_channels == 1 && input) {
			pcm.push(input, frames);
			return;
		}
		// Fixed-size scratch storage: no allocations, locks or host API calls
		// in the capture callback. Null input denotes a silent host buffer.
		std::array<std::int16_t, 512> converted{};
		std::size_t used = 0;
		auto emit = [&](std::int16_t value) {
			converted[used++] = value;
			if (used == converted.size()) {
				pcm.push(converted.data(), used);
				used = 0;
			}
		};
		for (std::size_t i = 0; i < frames; ++i) {
			std::int32_t current = input ? input[i * m_channels] : 0;
			if (input && m_channels == 2)
				current = (current + input[i * 2 + 1]) / 2;
			if (m_rate == 48000) {
				emit(static_cast<std::int16_t>(current));
				continue;
			}
			if (!m_have_previous) {
				m_previous = current;
				m_have_previous = true;
				continue;
			}
			// Integer phase is retained across arbitrary callback boundaries.
			// The following 48 kHz to 8 kHz FIR removes out-of-band energy.
			while (m_phase < 48000) {
				auto const sample = m_previous + (std::int64_t(current - m_previous) * m_phase) / 48000;
				emit(static_cast<std::int16_t>(sample));
				m_phase += m_rate;
			}
			m_phase -= 48000;
			m_previous = current;
		}
		if (used)
			pcm.push(converted.data(), used);
	}

private:
	unsigned m_rate = 48000;
	unsigned m_channels = 1;
	unsigned m_phase = 0;
	std::int32_t m_previous = 0;
	bool m_have_previous = false;
};

// PortAudio names are UTF-8. Fold ASCII case only, preserving other bytes.
inline bool millennium_mic_name_matches(std::string_view name, std::string_view prefix) noexcept
{
	if (prefix.size() > name.size())
		return false;
	auto fold = [](char ch) { return ch >= 'A' && ch <= 'Z' ? char(ch + ('a' - 'A')) : ch; };
	for (std::size_t i = 0; i < prefix.size(); ++i)
		if (fold(name[i]) != fold(prefix[i]))
			return false;
	return true;
}
