// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_model.h"

#include "millennium_mach_port_f.h"

#include <cmath>

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
	if (m_cfg.cpu_clock_hz > 0.0)
		m_cpu_hz = m_cfg.cpu_clock_hz;
}

void millennium_audio_model::reset()
{
	m_tone = 0;
	m_volume = 255;
	m_handset_loopback = false;
	m_handset = 0.f;
	m_dtmf_end_t = -1.0;
	m_dtmf_start_t = 0.0;
	m_dtmf_digit = '5';
	m_port_f_last_data = 0;
	m_port_f_last_edge_cycle = 0;
	m_port_f_half_period_cycles = 0;
	m_port_f_phase_origin_cycle = 0;
	m_port_f_have_half_period = false;
}

void millennium_audio_model::notify_mach_port_f(std::uint8_t prev_data, std::uint8_t next_data, std::uint64_t cpu_cycle,
	double cpu_hz)
{
	if (cpu_hz > 0.0)
		m_cpu_hz = cpu_hz;

	std::uint8_t const prev_low = static_cast<std::uint8_t>(prev_data & 0xffU);
	std::uint8_t const next_low = static_cast<std::uint8_t>(next_data & 0xffU);

	bool const prev_tone = (prev_low & millennium_mach_port_f::k_warning_tone_toggle_bit) != 0;
	bool const next_tone = (next_low & millennium_mach_port_f::k_warning_tone_toggle_bit) != 0;

	if (prev_tone != next_tone)
	{
		if (m_port_f_last_edge_cycle != 0)
		{
			std::uint64_t const delta = cpu_cycle - m_port_f_last_edge_cycle;
			if (delta > 0)
			{
				m_port_f_half_period_cycles = delta;
				m_port_f_have_half_period = true;
				m_port_f_phase_origin_cycle = m_port_f_last_edge_cycle;
			}
		}
		m_port_f_last_edge_cycle = cpu_cycle;
	}

	m_port_f_last_data = next_low;
}

float millennium_audio_model::mach_port_f_square_sample(double t_sec) const
{
	if ((m_port_f_last_data & millennium_mach_port_f::k_alarm_cadence_gate_bit) != 0)
		return 0.f;

	float constexpr amp = 0.22f;

	if (m_port_f_have_half_period && m_port_f_half_period_cycles > 0 && m_cpu_hz > 0.0)
	{
		double const cycle_d = t_sec * m_cpu_hz;
		std::uint64_t const cycle_t = static_cast<std::uint64_t>(cycle_d >= 0.0 ? cycle_d + 0.5 : cycle_d - 0.5);
		std::uint64_t const half = m_port_f_half_period_cycles;
		std::uint64_t const rel = cycle_t - m_port_f_phase_origin_cycle;
		bool const high = ((rel / half) & 1U) == 0U;
		return high ? amp : -amp;
	}

	bool const bit_on = (m_port_f_last_data & millennium_mach_port_f::k_warning_tone_toggle_bit) != 0;
	return bit_on ? amp : -amp;
}

void millennium_audio_model::write_tone_select(std::uint8_t value)
{
	m_tone = value & 0x0fU;
}

void millennium_audio_model::write_dtmf_digit(std::uint8_t ascii_digit, unsigned duration_ms)
{
	m_dtmf_digit = ascii_digit;
	m_dtmf_start_t = 0.0;
	m_dtmf_end_t = double(duration_ms) / 1000.0;
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
