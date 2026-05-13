// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace coinline::vfd {

class profile_11line_model {
public:
	void reset() noexcept;
	void set_active_page(std::uint8_t page) noexcept;
	void set_active_line(int line) noexcept;
	void set_blink_enabled(bool enabled) noexcept;
	void set_softkey_label(std::size_t idx, std::string const &label);

	std::uint8_t active_page() const noexcept { return m_active_page; }
	int active_line() const noexcept { return m_active_line; }
	bool blink_enabled() const noexcept { return m_blink_enabled; }
	std::string const &softkey_label(std::size_t idx) const noexcept { return m_softkey_labels[idx]; }

private:
	std::uint8_t m_active_page = 0U;
	int m_active_line = 0;
	bool m_blink_enabled = false;
	std::array<std::string, 10> m_softkey_labels{};
};

} // namespace coinline::vfd
