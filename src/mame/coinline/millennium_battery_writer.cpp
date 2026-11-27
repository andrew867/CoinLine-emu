// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_battery_writer.h"

#include <chrono>
#include <fstream>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

bool millennium_battery_writer::write_atomic(std::filesystem::path const &path,
	std::vector<std::uint8_t> const &image) noexcept
{
	try {
		std::error_code ec;
		if (!path.parent_path().empty())
			std::filesystem::create_directories(path.parent_path(), ec);
		if (ec)
			return false;
		auto tmp = path;
		tmp += ".tmp";
		{
			std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
			if (!out)
				return false;
			out.write(reinterpret_cast<char const *>(image.data()), std::streamsize(image.size()));
			out.close();
			if (!out)
				return false;
		}
#if defined(_WIN32)
		return ::MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		std::filesystem::rename(tmp, path, ec);
		return !ec;
#endif
	} catch (...) {
		return false;
	}
}

bool millennium_battery_writer::submit(std::filesystem::path path, std::vector<std::uint8_t> image) noexcept
{
	if (m_pending.valid())
		return false;
	try {
		m_pending = std::async(std::launch::async, [path = std::move(path), image = std::move(image)] {
			return write_atomic(path, image);
		});
		return true;
	} catch (...) {
		return false;
	}
}

millennium_battery_writer::result millennium_battery_writer::poll() noexcept
{
	if (!m_pending.valid())
		return result::idle;
	if (m_pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return result::busy;
	return finish();
}

millennium_battery_writer::result millennium_battery_writer::finish() noexcept
{
	if (!m_pending.valid())
		return result::idle;
	try {
		return m_pending.get() ? result::saved : result::failed;
	} catch (...) {
		return result::failed;
	}
}
