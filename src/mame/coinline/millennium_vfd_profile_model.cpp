// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_vfd_profile_model.h"

#include <algorithm>

namespace coinline::vfd {

void profile_11line_model::reset() noexcept
{
	m_active_page = 0U;
	m_active_line = 0;
	m_blink_enabled = false;
	m_softkey_labels.fill(std::string{});
}

void profile_11line_model::set_active_page(std::uint8_t page) noexcept
{
	m_active_page = static_cast<std::uint8_t>(page & 0x03U);
}

void profile_11line_model::set_active_line(int line) noexcept
{
	m_active_line = std::clamp(line, 0, 10);
}

void profile_11line_model::set_blink_enabled(bool enabled) noexcept
{
	m_blink_enabled = enabled;
}

void profile_11line_model::set_softkey_label(std::size_t idx, std::string const &label)
{
	if (idx >= m_softkey_labels.size())
		return;
	m_softkey_labels[idx] = label;
}

} // namespace coinline::vfd
