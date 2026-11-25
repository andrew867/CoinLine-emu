// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <string>

// Four-page column-addressed expansion display and its physical key scanner.
class millennium_graphic_vfd {
public:
	static constexpr unsigned bands = 11;
	static constexpr unsigned stride = 128;
	static constexpr unsigned width = 126;
	static constexpr unsigned height = bands * 8;
	static constexpr unsigned page_size = bands * stride;
	static constexpr unsigned page_count = 4;

	static bool pixel_address(std::uint16_t port) noexcept;
	void configure(unsigned write_cycles, unsigned clear_cycles, bool lamp_active_high) noexcept;
	void reset() noexcept;
	void release_reset() noexcept;
	void write_control(std::uint8_t value, std::uint64_t cycle) noexcept;
	void write_pixel(std::uint16_t port, std::uint8_t value, std::uint64_t cycle) noexcept;
	void write_keys(std::uint8_t value) noexcept { m_key_select = value; }
	std::uint8_t read_keys(std::uint8_t other, std::uint16_t pressed) const noexcept;
	std::uint8_t read_status(std::uint8_t other, std::uint64_t cycle) const noexcept;
	bool lit(unsigned x, unsigned y) const noexcept;
	unsigned visible_page() const noexcept { return (m_control >> 2) & 3; }
	unsigned write_page() const noexcept { return ((m_control >> 5) & 2) | m_low_page; }
	std::uint8_t byte(unsigned page, unsigned offset) const noexcept { return m_pages[page * page_size + offset]; }
	std::uint8_t control() const noexcept { return m_control; }
	std::uint8_t brightness() const noexcept { return m_brightness; }
	std::uint8_t key_select() const noexcept { return m_key_select; }
	bool enabled() const noexcept { return m_released && (m_control & 0x10); }
	// Packed visible page and optical state form a cheap frame-cache key.
	std::uint8_t const *visible_page_data() const noexcept { return m_pages.data() + visible_page() * page_size; }
	std::uint8_t optical_state() const noexcept { return !enabled() ? 0 : bool(m_control & 0x20) == m_lamp_active_high ? 2 : 1; }
	std::string snapshot_json() const;

private:
	friend class millennium_state;
	std::array<std::uint8_t, page_count * page_size> m_pages{};
	std::uint8_t m_control = 3;
	std::uint8_t m_low_page = 0;
	std::uint8_t m_brightness = 0xff;
	std::uint8_t m_key_select = 0;
	bool m_released = false;
	std::uint64_t m_busy_until = 0;
	unsigned m_write_cycles = 0;
	unsigned m_clear_cycles = 0;
	bool m_lamp_active_high = true;
};
