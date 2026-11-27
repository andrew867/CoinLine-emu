// license:BSD-3-Clause
#pragma once

#include <algorithm>
#include <cstdint>

// Stand-alone PCM/waveform-element records. The clock and element lengths
// are inferred from ROM timing and spectral measurements. Gain, the first
// element byte and trailing silence timing remain provisional.
namespace upd7759_waveform {

inline constexpr std::uint32_t bank_size = 0x20000;
inline constexpr std::uint32_t max_record_clocks = 640000U * 120U;
inline constexpr std::uint8_t lengths[] = {12, 16, 8, 48, 96, 32, 24, 64};

struct element {
	std::uint32_t samples_offset = 0;
	std::uint32_t points = 0;
	std::uint32_t clocks = 0;
	std::uint16_t divider = 0;
	std::uint8_t length = 0;
	std::uint8_t gain = 0;
};

enum class result { block, end, invalid };

constexpr std::uint16_t clock_divider(std::uint8_t parameter) noexcept
{
	return 4U * ((parameter & 0x3fU) + 1U) + (parameter >> 6U);
}

// Prevent a malformed message from reading into the next directory entry or
// wrapping around the physical ROM bank. Aliases to the same record are valid.
template <typename Read>
std::uint32_t record_limit(Read read, std::uint32_t offset, std::uint32_t bank_end)
{
	std::uint32_t limit = bank_end;
	unsigned const count = unsigned(read(0)) + 1U;
	if (5U + 2U * count > bank_end)
		return offset;
	for (unsigned i = 0; i < count; ++i) {
		std::uint32_t const candidate = (std::uint32_t(read(5U + 2U * i)) << 9U)
			| (std::uint32_t(read(6U + 2U * i)) << 1U);
		if (candidate > offset && candidate < limit)
			limit = candidate;
	}
	return limit;
}

template <typename Read>
result next_element(Read read, std::uint32_t &offset, std::uint32_t limit,
	std::uint16_t &divider, std::uint8_t &gain, element &out)
{
	out = {};
	while (offset < limit) {
		std::uint8_t const op = read(offset++);
		if (op == 0)
			return result::end;
		if (op >= 0xc0) {
			if (offset >= limit)
				return result::invalid;
			divider = clock_divider(read(offset++));
			gain = op & 0x3fU;
			continue;
		}
		if (op == 0x2f) {
			// Observed trailing pause, using the speech engine's silence clock
			// units until waveform-specific timing can be measured on hardware.
			out.clocks = 1024U * ((op & 0x3fU) + 1U);
			return result::block;
		}
		if ((op >= 0x40 && op < 0x80) || (op >= 1 && op <= 7)) {
			if (divider == 0 || offset >= limit)
				return result::invalid;
			out.length = lengths[op & 7U];
			unsigned const repetitions = unsigned(read(offset++)) + 1U;
			if (out.length > limit - offset)
				return result::invalid;
			out.samples_offset = offset;
			out.points = repetitions * out.length;
			out.divider = divider;
			out.gain = gain;
			out.clocks = out.points * divider;
			offset += out.length;
			return result::block;
		}
		return result::invalid;
	}
	return result::invalid;
}

// The first position appears to be a control byte; treating that position
// as the DAC midpoint avoids a spurious impulse in each repeated waveform.
// The linear gain interpretation is not yet calibrated to a physical DAC.
constexpr std::int16_t dac_sample(std::uint8_t value, std::uint8_t point, std::uint8_t gain) noexcept
{
	return point == 0 ? 0 : std::int16_t((int(value) - 128) * 2 * int(std::min<unsigned>(gain, 31U)) / 31);
}

template <typename Read>
bool validate(Read read, std::uint32_t offset, std::uint32_t limit, std::uint32_t &clocks)
{
	clocks = 0;
	std::uint16_t divider = 0;
	std::uint8_t gain = 0;
	bool have_wave = false;
	for (;;) {
		element block;
		switch (next_element(read, offset, limit, divider, gain, block)) {
		case result::invalid: return false;
		case result::end: return have_wave;
		case result::block:
			if (block.clocks > max_record_clocks - clocks)
				return false;
			clocks += block.clocks;
			have_wave = have_wave || block.length != 0;
			break;
		}
	}
}
} // namespace upd7759_waveform
