// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// MACH port \c F (\c 0xC3) lower bits drive card-reader control and audible-warning outputs on this board family.
namespace millennium_mach_port_f {

inline constexpr std::uint8_t k_card_reader_control_bit = 0x01U;
inline constexpr std::uint8_t k_warning_tone_toggle_bit = 0x02U; ///< Square-wave drive for forgotten-card / payment-warning cadence (ISR toggles).
inline constexpr std::uint8_t k_alarm_cadence_gate_bit = 0x08U;   ///< Cadence on/off segment when paired with timers.

} // namespace millennium_mach_port_f
