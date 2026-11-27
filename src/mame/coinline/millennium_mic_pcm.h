// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

// One capture thread writes 48 kHz PCM and one audio thread reads 8 kHz PCM.
class millennium_mic_pcm {
public:
	// Only used before starting a host stream, while neither endpoint runs.
	void reset() noexcept
	{
		m_write.store(0, std::memory_order_relaxed);
		m_read.store(0, std::memory_order_relaxed);
		m_primed = false;
	}

	void push(std::int16_t const *samples, std::size_t count) noexcept
	{
		auto const write = m_write.load(std::memory_order_relaxed);
		auto const read = m_read.load(std::memory_order_acquire);
		count = std::min(count, m_samples.size() - (write - read));
		for (std::size_t i = 0; i < count; ++i)
			m_samples[(write + i) & mask] = samples[i];
		m_write.store(write + static_cast<std::uint32_t>(count), std::memory_order_release);
	}

	float read() noexcept
	{
		auto const write = m_write.load(std::memory_order_acquire);
		auto read = m_read.load(std::memory_order_relaxed);
		if (write - read > max_backlog)
			read = write - start_backlog;
		if (write - read < filter.size())
			m_primed = false;
		if (!m_primed && write - read < start_backlog)
			return 0.f;
		m_primed = true;

		float sum = 0.f;
		for (std::size_t i = 0; i < filter.size(); ++i)
			sum += filter[i] * m_samples[(read + i) & mask];
		m_read.store(read + 6, std::memory_order_release);
		return sum / 32768.f;
	}

private:
	static constexpr std::uint32_t capacity = 8192;
	static constexpr std::uint32_t mask = capacity - 1;
	static constexpr std::uint32_t start_backlog = 1920; // 40 ms
	static constexpr std::uint32_t max_backlog = 4800; // 100 ms

	// Unity-gain 3.4 kHz Hamming-windowed low-pass filter at 48 kHz.
	static constexpr std::array filter{
		-0.001009631f, -0.000843057f, -0.000496339f,  0.000136854f,  0.001127633f,  0.002418234f,
		 0.003752490f,  0.004669695f,  0.004585565f,  0.002957239f, -0.000501156f, -0.005610485f,
		-0.011594387f, -0.017081766f, -0.020274224f, -0.019267019f, -0.012467853f,  0.000976626f,
		 0.020850879f,  0.045729378f,  0.073079731f,  0.099611225f,  0.121811740f,  0.136568178f,
		 0.141740902f,  0.136568178f,  0.121811740f,  0.099611225f,  0.073079731f,  0.045729378f,
		 0.020850879f,  0.000976626f, -0.012467853f, -0.019267019f, -0.020274224f, -0.017081766f,
		-0.011594387f, -0.005610485f, -0.000501156f,  0.002957239f,  0.004585565f,  0.004669695f,
		 0.003752490f,  0.002418234f,  0.001127633f,  0.000136854f, -0.000496339f, -0.000843057f,
		-0.001009631f,
	};

	std::array<std::int16_t, capacity> m_samples{};
	std::atomic<std::uint32_t> m_write{0};
	std::atomic<std::uint32_t> m_read{0};
	bool m_primed = false;
};
