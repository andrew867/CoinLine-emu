// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_model.h"

#include "millennium_mach_port_f.h"

#include <cmath>
#include <cstdint>
#include <iostream>

int main()
{
	double constexpr hz = 12288000.0;
	millennium_audio_model m;
	millennium_alerter_board_config cfg{};
	cfg.cpu_clock_hz = hz;
	m.configure(cfg);
	m.reset();
	m.write_tone_select(0);
	m.write_volume(255);

	// Two opposite edges 3000 cycles apart → 3000-cycle half-period square wave.
	m.notify_mach_port_f(0, millennium_mach_port_f::k_warning_tone_toggle_bit, 1000ULL, hz);
	m.notify_mach_port_f(millennium_mach_port_f::k_warning_tone_toggle_bit, 0, 4000ULL, hz);

	double const t_mid_high = 2500.0 / hz;
	double const t_mid_low = 4500.0 / hz;
	float const y_hi = m.output_sample(t_mid_high);
	float const y_lo = m.output_sample(t_mid_low);
	if (!(y_hi > 0.05f && y_lo < -0.05f)) {
		std::cerr << "expected alternating port-F square samples\n";
		return 1;
	}

	// Cadence gate bit set → mute GPIO square (still models firmware clearing segment).
	m.notify_mach_port_f(0, std::uint8_t(millennium_mach_port_f::k_warning_tone_toggle_bit |
					       millennium_mach_port_f::k_alarm_cadence_gate_bit),
		5000ULL, hz);
	float const y_muted = std::fabs(m.output_sample(6000.0 / hz));
	if (y_muted > 0.02f) {
		std::cerr << "expected silence when cadence gate bit is set\n";
		return 1;
	}

	return 0;
}
