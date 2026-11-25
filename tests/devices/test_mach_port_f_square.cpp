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

	using namespace millennium_mach_port_f;
	constexpr auto select = k_warning_source_select_bit;
	constexpr auto wave = k_warning_tone_toggle_bit;
	constexpr auto gate = k_alarm_cadence_gate_bit;
	// Card power-latch activity with the ring path selected remains silent.
	m.notify_mach_port_f(0, wave, 1000.0 / hz);
	m.notify_mach_port_f(wave, 0, 4000.0 / hz);
	if (m.warning_selected() || m.alerter_speaker_sample(4500.0 / hz) != 0.f) return 1;
	m.notify_mach_port_f(0, select, 5000.0 / hz);
	if (!m.warning_selected() || m.alerter_speaker_sample(5500.0 / hz) != 0.f) return 1;
	// Actual alternating edges drive the enclosure, never the handset.
	m.notify_mach_port_f(select, select | wave, 6000.0 / hz);
	m.notify_mach_port_f(select | wave, select, 9000.0 / hz);
	float const y_lo = m.alerter_speaker_sample(9500.0 / hz);
	m.notify_mach_port_f(select, select | wave, 12000.0 / hz);
	float const y_hi = m.alerter_speaker_sample(12500.0 / hz);
	if (!(y_hi > 0.05f && y_lo < -0.05f)) {
		std::cerr << "expected alternating port-F square samples\n";
		return 1;
	}

	// Cadence gate bit set → mute GPIO square (still models firmware clearing segment).
	if (m.handset_speaker_sample(12500.0 / hz) != 0.f) return 1;
	m.notify_mach_port_f(select | wave, select | gate, 15000.0 / hz);
	float const y_muted = std::fabs(m.alerter_speaker_sample(15500.0 / hz));
	if (y_muted > 0.02f) {
		std::cerr << "expected silence when cadence gate bit is set\n";
		return 1;
	}

	m.notify_mach_port_f(select | gate, select | wave, 18000.0 / hz);
	if (m.alerter_speaker_sample(18500.0 / hz) <= 0.f || m.alerter_speaker_sample(25000.0 / hz) != 0.f) return 1;
	m.notify_mach_port_f(select | wave, wave, 26000.0 / hz);
	if (m.warning_selected() || m.alerter_speaker_sample(26500.0 / hz) != 0.f) return 1;

	// Known legacy Desk profile selects a separate nominal 3 kHz oscillator.
	cfg.enclosure = millennium_enclosure_profile::desk_oscillator;
	m.configure(cfg); m.reset();
	m.set_legacy_warning(true);
	if (!m.warning_selected() || m.alerter_speaker_sample(1.0 / 12000.0) <= 0.f ||
		m.alerter_speaker_sample(3.0 / 12000.0) >= 0.f) return 1;
	if (m.handset_speaker_sample(1.0 / 12000.0) != 0.f) return 1;
	m.set_legacy_warning(false);
	if (m.warning_selected() || m.alerter_speaker_sample(1.0 / 12000.0) != 0.f) return 1;
	return 0;
}
