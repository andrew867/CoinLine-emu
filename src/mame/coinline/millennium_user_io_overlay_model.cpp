// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_user_io_overlay_model.h"

namespace coinline::userio {

namespace {

bool is_quick_key(std::uint8_t raw_keycode) noexcept
{
	return raw_keycode >= 0xA0U && raw_keycode <= 0xAFU;
}

bool is_numeric_key(std::uint8_t raw_keycode) noexcept
{
	return raw_keycode <= 9U;
}

} // namespace

consumer arbitrate_ordered_consumer(overlay_chain_state const &state, std::uint8_t raw_keycode) noexcept
{
	if (state.overlay_enabled)
		return consumer::overlay_profile;
	if (state.quick_key_enabled && is_quick_key(raw_keycode))
		return consumer::quick_key;
	if (state.numeric_enabled && is_numeric_key(raw_keycode))
		return consumer::numeric_keypad;
	return consumer::system_fallback;
}

} // namespace coinline::userio
