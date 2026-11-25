// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_model.h"

#include "millennium_mach_port_f.h"

#include <cmath>
#include <cstdint>

namespace {

// Must stay aligned with `fixtures/audio/alerter-service-beep.json` / `alerter-error-beep.json` `edges_ms`.
unsigned const g_service_edges[] = { 0, 65, 115, 180 };
unsigned const g_error_edges[] = { 0, 250 };
unsigned const g_user_prompt_edges[] = { 0, 45, 90, 135, 180, 225 };

float constexpr pi = 3.14159265f;

float sin_hz(double t, double hz)
{
	return static_cast<float>(std::sin(2.0 * pi * double(hz) * t));
}

float clampf(float x, float lo, float hi)
{
	return x < lo ? lo : (x > hi ? hi : x);
}

// ITU-T Q.23 low-group / high-group Hz (row x column).
double constexpr row_hz[4] = {697.0, 770.0, 852.0, 941.0};
double constexpr col_hz[4] = {1209.0, 1336.0, 1477.0, 1633.0};

} // namespace

unsigned millennium_audio_model::cadence_edge_count(millennium_alerter_cadence_kind k)
{
	switch (k) {
	case millennium_alerter_cadence_kind::service_beep:
		return unsigned(sizeof(g_service_edges) / sizeof(g_service_edges[0]));
	case millennium_alerter_cadence_kind::error_beep:
		return unsigned(sizeof(g_error_edges) / sizeof(g_error_edges[0]));
	case millennium_alerter_cadence_kind::user_prompt_tone:
		return unsigned(sizeof(g_user_prompt_edges) / sizeof(g_user_prompt_edges[0]));
	default:
		return 0;
	}
}

unsigned millennium_audio_model::cadence_edge_ms(millennium_alerter_cadence_kind k, unsigned index)
{
	switch (k) {
	case millennium_alerter_cadence_kind::service_beep: {
		unsigned const n = unsigned(sizeof(g_service_edges) / sizeof(g_service_edges[0]));
		return index < n ? g_service_edges[index] : 0U;
	}
	case millennium_alerter_cadence_kind::error_beep: {
		unsigned const n = unsigned(sizeof(g_error_edges) / sizeof(g_error_edges[0]));
		return index < n ? g_error_edges[index] : 0U;
	}
	case millennium_alerter_cadence_kind::user_prompt_tone: {
		unsigned const n = unsigned(sizeof(g_user_prompt_edges) / sizeof(g_user_prompt_edges[0]));
		return index < n ? g_user_prompt_edges[index] : 0U;
	}
	default:
		return 0;
	}
}

void millennium_audio_model::configure(millennium_alerter_board_config const &cfg)
{
	m_cfg = cfg;
	if (m_cfg.sample_rate_hz == 0)
		m_cfg.sample_rate_hz = 8000;
}

void millennium_audio_model::prepare_high_fast_ringer(std::uint32_t sample_rate)
{
	if (sample_rate < 8000U || sample_rate > 192000U)
		return;
	double constexpr k_pi = 3.14159265358979323846;
	double constexpr k_freq[2] = {2000.0 / 3.0, 500.0};
	std::uint32_t constexpr k_us[2] = {50000U, 50000U};
	double constexpr k_duty = 0.50;
	double const limit = std::fmin(5200.0, 0.45 * static_cast<double>(sample_rate));
	std::uint32_t us = 0;
	for (unsigned stage = 0; stage < 2U; ++stage) {
		double const frequency = k_freq[stage];
		m_ring_steps[stage] = static_cast<std::uint32_t>(frequency * 4294967296.0 / static_cast<double>(sample_rate) + 0.5);
		us += k_us[stage];
		m_ring_stage_ends[stage] = static_cast<std::uint32_t>((static_cast<std::uint64_t>(us) * sample_rate + 500000ULL) / 1000000ULL);
		for (unsigned i = 0; i < 256U; ++i) {
			double const position = static_cast<double>(i) / 256.0;
			double sample = 0.0;
			for (unsigned harmonic = 1; harmonic * frequency < limit; ++harmonic) {
				double const taper = std::cos(k_pi * harmonic * frequency / (2.0 * limit));
				double const amplitude = 4.0 * std::sin(k_pi * harmonic * k_duty) / (k_pi * harmonic);
				sample += taper * amplitude * std::cos(2.0 * k_pi * harmonic * (position - k_duty / 2.0));
			}
			m_ring_table[stage][i] = static_cast<std::int16_t>(std::lround(sample * 6000.0));
		}
	}
	m_ringer_ready = true;
	reset_high_fast_ringer();
}

void millennium_audio_model::reset_high_fast_ringer() noexcept
{
	m_ring_phase = 0;
	m_ring_elapsed = 0;
	m_ring_stage = 0;
}

float millennium_audio_model::pull_high_fast_ringer() noexcept
{
	if (!m_ringer_ready)
		return 0.f;
	unsigned const index = m_ring_phase >> 24;
	float const sample = static_cast<float>(m_ring_table[m_ring_stage][index]) / 32768.f;
	m_ring_phase += m_ring_steps[m_ring_stage];
	if (++m_ring_elapsed == m_ring_stage_ends[m_ring_stage]) {
		if (++m_ring_stage == 2U) {
			m_ring_stage = 0;
			m_ring_elapsed = 0;
		}
	}
	return sample;
}

void millennium_audio_model::reset()
{
	m_tone = 0;
	m_volume = 255;
	m_handset_loopback = false;
	m_rx_muted = false;
	m_rx_gain = 1.f;
	m_handset = 0.f;
	m_dtmf_end_t = -1.0;
	m_dtmf_start_t = 0.0;
	m_dtmf_digit = '5';
	m_port_f_last_data = 0;
	m_port_f_last_edge_time = -1.0;
	m_port_f_half_period = 0.0;
	m_legacy_warning = false;
	reset_high_fast_ringer();
}

void millennium_audio_model::notify_mach_port_f(std::uint8_t prev_data, std::uint8_t next_data, double event_time)
{
	std::uint8_t const prev_low = static_cast<std::uint8_t>(prev_data & 0xffU);
	std::uint8_t const next_low = static_cast<std::uint8_t>(next_data & 0xffU);
	bool const selected = (next_low & millennium_mach_port_f::k_warning_source_select_bit) != 0;
	bool const was_selected = (prev_low & millennium_mach_port_f::k_warning_source_select_bit) != 0;
	// Edges used for the card supply latch must not seed an alarm oscillator.
	if (m_cfg.enclosure != millennium_enclosure_profile::timer_warning || !selected || !was_selected) {
		m_port_f_last_edge_time = -1.0;
		m_port_f_half_period = 0.0;
	}

	bool const prev_tone = (prev_low & millennium_mach_port_f::k_warning_tone_toggle_bit) != 0;
	bool const next_tone = (next_low & millennium_mach_port_f::k_warning_tone_toggle_bit) != 0;

	if (selected && m_cfg.enclosure == millennium_enclosure_profile::timer_warning && prev_tone != next_tone)
	{
		if (m_port_f_last_edge_time >= 0.0 && event_time > m_port_f_last_edge_time) {
			double const interval = event_time - m_port_f_last_edge_time;
			m_port_f_half_period = interval <= 0.02 ? interval : 0.0;
		}
		m_port_f_last_edge_time = event_time;
	}

	m_port_f_last_data = next_low;
}

bool millennium_audio_model::warning_selected() const noexcept
{
	if (m_cfg.enclosure == millennium_enclosure_profile::desk_oscillator)
		return m_legacy_warning;
	return m_cfg.enclosure == millennium_enclosure_profile::timer_warning &&
		(m_port_f_last_data & millennium_mach_port_f::k_warning_source_select_bit) != 0;
}

float millennium_audio_model::mach_port_f_square_sample(double t_sec) const
{
	if (!warning_selected()) return 0.f;
	float constexpr amp = 0.22f;
	if (m_cfg.enclosure == millennium_enclosure_profile::desk_oscillator)
		return sin_hz(t_sec, 3000.0) >= 0.f ? amp : -amp;
	if ((m_port_f_last_data & millennium_mach_port_f::k_alarm_cadence_gate_bit) != 0)
		return 0.f;
	if (m_port_f_half_period > 0.0) {
		double const age = t_sec - m_port_f_last_edge_time;
		// Do not free-run an oscillator after the physical timer stops toggling.
		if (age >= 0.0 && age <= 2.0 * m_port_f_half_period)
			return (m_port_f_last_data & millennium_mach_port_f::k_warning_tone_toggle_bit) ? amp : -amp;
	}
	return 0.f;
}

void millennium_audio_model::write_tone_select(std::uint8_t value)
{
	m_tone = value & 0x0fU;
}

void millennium_audio_model::write_dtmf_digit(std::uint8_t ascii_digit, unsigned duration_ms, double now_sec)
{
	m_dtmf_digit = ascii_digit;
	m_dtmf_start_t = now_sec;
	m_dtmf_end_t = now_sec + double(duration_ms) / 1000.0;
}

void millennium_audio_model::write_volume(std::uint8_t value)
{
	m_volume = value;
}

void millennium_audio_model::dtmf_row_col_for_digit(std::uint8_t ascii, int &row_out, int &col_out)
{
	switch (ascii) {
	case '1':
		row_out = 0;
		col_out = 0;
		return;
	case '2':
		row_out = 0;
		col_out = 1;
		return;
	case '3':
		row_out = 0;
		col_out = 2;
		return;
	case 'A':
	case 'a':
		row_out = 0;
		col_out = 3;
		return;
	case '4':
		row_out = 1;
		col_out = 0;
		return;
	case '5':
		row_out = 1;
		col_out = 1;
		return;
	case '6':
		row_out = 1;
		col_out = 2;
		return;
	case 'B':
	case 'b':
		row_out = 1;
		col_out = 3;
		return;
	case '7':
		row_out = 2;
		col_out = 0;
		return;
	case '8':
		row_out = 2;
		col_out = 1;
		return;
	case '9':
		row_out = 2;
		col_out = 2;
		return;
	case 'C':
	case 'c':
		row_out = 2;
		col_out = 3;
		return;
	case '*':
		row_out = 3;
		col_out = 0;
		return;
	case '0':
		row_out = 3;
		col_out = 1;
		return;
	case '#':
		row_out = 3;
		col_out = 2;
		return;
	case 'D':
	case 'd':
		row_out = 3;
		col_out = 3;
		return;
	default:
		row_out = 1;
		col_out = 1;
		return;
	}
}

float millennium_audio_model::dtmf_sample(double t_sec) const
{
	int row = 0, col = 0;
	dtmf_row_col_for_digit(m_dtmf_digit, row, col);
	double const tr = row_hz[row];
	double const tc = col_hz[col];
	return 0.5f * (sin_hz(t_sec, tr) + sin_hz(t_sec, tc));
}

float millennium_audio_model::progress_tone_sample(double t_sec) const
{
	switch (m_tone) {
	case 0:
		return 0.f;
	case 1: { // dial: 350 + 440 Hz continuous
		return 0.5f * (sin_hz(t_sec, 350.0) + sin_hz(t_sec, 440.0));
	}
	case 2: { // busy: 480 + 620 Hz, 0.5 s on / 0.5 s off
		double const p = std::fmod(t_sec, 1.0);
		float const env = (p < 0.5) ? 1.f : 0.f;
		return env * 0.5f * (sin_hz(t_sec, 480.0) + sin_hz(t_sec, 620.0));
	}
	case 3: { // ringback: 440 + 480 Hz, 2 s on / 4 s off
		double const p = std::fmod(t_sec, 6.0);
		float const env = (p < 2.0) ? 1.f : 0.f;
		return env * 0.5f * (sin_hz(t_sec, 440.0) + sin_hz(t_sec, 480.0));
	}
	case 4: { // reorder (fast busy): same freqs as busy, 0.25 on / 0.25 off
		double const p = std::fmod(t_sec, 0.5);
		float const env = (p < 0.25) ? 1.f : 0.f;
		return env * 0.5f * (sin_hz(t_sec, 480.0) + sin_hz(t_sec, 620.0));
	}
	case 5: { // off-hook warning: four tones, 0.1 s on / 0.1 s off
		double const p = std::fmod(t_sec, 0.2);
		float const env = (p < 0.1) ? 1.f : 0.f;
		float const s =
			0.25f * (sin_hz(t_sec, 1400.0) + sin_hz(t_sec, 2060.0) + sin_hz(t_sec, 2450.0) + sin_hz(t_sec, 2600.0));
		return env * s;
	}
	default:
		return 0.f;
	}
}

float millennium_audio_model::alerter_speaker_sample(double t_sec) const
{
	return mach_port_f_square_sample(t_sec);
}

float millennium_audio_model::handset_speaker_sample(double t_sec) const
{
	bool const dtmf = m_dtmf_end_t > 0.0 && t_sec >= m_dtmf_start_t && t_sec < m_dtmf_end_t;
	float y = 0.f;
	if (dtmf)
		y = dtmf_sample(t_sec - m_dtmf_start_t);
	else if (!m_rx_muted)
		y = progress_tone_sample(t_sec);
	float const g = float(m_volume) / 255.f;
	y *= g;
	if (!m_rx_muted && !dtmf)
		y += m_handset * 0.5f;
	return clampf(y * m_rx_gain, -1.f, 1.f);
}

float millennium_audio_model::output_sample(double t_sec) const
{
	float const pf = mach_port_f_square_sample(t_sec);
	float y = 0.f;
	if (m_dtmf_end_t > 0.0 && t_sec >= m_dtmf_start_t && t_sec < m_dtmf_end_t)
		y = dtmf_sample(t_sec - m_dtmf_start_t) + pf;
	else
		y = progress_tone_sample(t_sec) + pf;

	float const g = float(m_volume) / 255.f;
	y *= g;
	if (m_handset_loopback)
		y += m_handset * 0.5f;
	return clampf(y, -1.f, 1.f);
}
