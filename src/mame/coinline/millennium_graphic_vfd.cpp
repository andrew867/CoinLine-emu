// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_graphic_vfd.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

bool millennium_graphic_vfd::pixel_address(std::uint16_t port) noexcept
{
	return (port >= 0x1280 && port <= 0x17ff) || (port >= 0x1a80 && port <= 0x1fff);
}

void millennium_graphic_vfd::configure(unsigned write_cycles, unsigned clear_cycles, bool lamp_active_high) noexcept
{
	m_write_cycles = write_cycles;
	m_clear_cycles = clear_cycles;
	m_lamp_active_high = lamp_active_high;
}

void millennium_graphic_vfd::reset() noexcept
{
	m_pages.fill(0);
	m_control = 3;
	m_low_page = 0;
	m_brightness = 0xff;
	m_key_select = 0;
	m_released = false;
	m_busy_until = 0;
}

void millennium_graphic_vfd::release_reset() noexcept
{
	m_released = true;
}

void millennium_graphic_vfd::write_control(std::uint8_t value, std::uint64_t cycle) noexcept
{
	bool const clear = (m_control & 1) && !(value & 1);
	m_control = value;
	if (m_released && clear) {
		std::fill_n(m_pages.begin() + write_page() * page_size, page_size, 0);
		m_busy_until = cycle + m_clear_cycles;
	}
}

void millennium_graphic_vfd::write_pixel(std::uint16_t port, std::uint8_t value, std::uint64_t cycle) noexcept
{
	if (!m_released || !pixel_address(port))
		return;
	m_low_page = port >= 0x1a80 ? 1 : 0;
	unsigned const offset = port - (m_low_page ? 0x1a80 : 0x1280);
	if (m_control & 2)
		m_pages[write_page() * page_size + offset] = value;
	else if (offset == 0)
		m_brightness = value;
	m_busy_until = cycle + m_write_cycles;
}

std::uint8_t millennium_graphic_vfd::read_status(std::uint8_t other, std::uint64_t cycle) const noexcept
{
	return (other & 0xfe) | (m_released && cycle >= m_busy_until ? 1 : 0);
}

std::uint8_t millennium_graphic_vfd::read_keys(std::uint8_t other, std::uint16_t pressed) const noexcept
{
	std::uint8_t inputs = 0x78;
	for (unsigned group = 0; group < 3; ++group) {
		if (!(m_key_select & (1U << group)))
			continue;
		for (unsigned position = 0; position < 4; ++position)
			if (pressed & (1U << (position * 3 + group)))
				inputs &= ~(1U << (position + 3));
	}
	return (other & ~0x78) | inputs;
}

bool millennium_graphic_vfd::lit(unsigned x, unsigned y) const noexcept
{
	if (x >= width || y >= height || !enabled())
		return false;
	if (bool(m_control & 0x20) == m_lamp_active_high)
		return true;
	unsigned const offset = (bands - 1 - y / 8) * stride + x;
	return (byte(visible_page(), offset) & (1U << (y % 8))) != 0;
}

std::string millennium_graphic_vfd::snapshot_json() const
{
	std::ostringstream out;
	out << "{\"variant\":\"11line\",\"width\":" << width << ",\"height\":" << height
		<< ",\"stride\":" << stride << ",\"visible_page\":" << visible_page()
		<< ",\"write_page\":" << write_page() << ",\"control\":" << unsigned(m_control)
		<< ",\"brightness\":" << unsigned(m_brightness) << ",\"enabled\":" << (enabled() ? "true" : "false")
		<< ",\"pages_hex\":[";
	for (unsigned page = 0; page < page_count; ++page) {
		if (page)
			out << ',';
		out << '"';
		for (unsigned offset = 0; offset < page_size; ++offset)
			out << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte(page, offset));
		out << '"';
	}
	out << "]}\n";
	return out.str();
}
