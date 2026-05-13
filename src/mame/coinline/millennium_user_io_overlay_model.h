// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

namespace coinline::userio {

enum class consumer : std::uint8_t {
	none = 0,
	overlay_profile,
	quick_key,
	numeric_keypad,
	system_fallback,
};

struct overlay_chain_state {
	bool overlay_enabled = false;
	bool quick_key_enabled = false;
	bool numeric_enabled = true;
};

consumer arbitrate_ordered_consumer(overlay_chain_state const &state, std::uint8_t raw_keycode) noexcept;

} // namespace coinline::userio
