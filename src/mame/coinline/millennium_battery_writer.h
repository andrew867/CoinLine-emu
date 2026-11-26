// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <vector>

// One immutable host-storage snapshot at a time. The emulation thread owns
// live RAM; the worker receives a copy and never touches device state.
class millennium_battery_writer {
public:
	enum class result { idle, busy, saved, failed };

	bool submit(std::filesystem::path path, std::vector<std::uint8_t> image) noexcept;
	result poll() noexcept;
	result finish() noexcept;

	// Keep the last complete battery image in place until replacement succeeds.
	// Used by the worker and for the final synchronous snapshot at shutdown.
	static bool write_atomic(std::filesystem::path const &path, std::vector<std::uint8_t> const &image) noexcept;

private:
	std::future<bool> m_pending;
};
