// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

enum class millennium_alerter_cadence_kind : std::uint8_t {
	none = 0,
	service_beep,
	error_beep,
	user_prompt_tone,
};

struct millennium_alerter_board_config {
	unsigned sample_rate_hz = 8000;
	/// When zero, emulator uses \c maincpu \c unscaled_clock() (\c millennium_audio_device::device_start).
	double cpu_clock_hz = 0.0;
	/// Lower nibble of tone-select port (`0x58`) that triggers table-driven cadence traces (placeholders).
	std::uint8_t service_tone_nibble = 0x0EU;
	std::uint8_t error_tone_nibble = 0x0FU;
	std::uint8_t user_prompt_tone_nibble = 0x0DU;
};

/// Call-progress / DTMF for the alerter ports (\c 0x58–\c 0x5b), plus optional **square-wave** contribution when the CPU
/// toggles MACH port \c F bit **1** (ISR-driven forgotten-card / payment-warning cadence). Cadence **mute** when bit **3**
/// is set (hardware clears it during “sound on” segments). Phrase/voice prompts are modeled separately (PCM path).
class millennium_audio_model {
public:
	void configure(millennium_alerter_board_config const &cfg);
	void reset();

	void write_tone_select(std::uint8_t value);
	void write_dtmf_digit(std::uint8_t ascii_digit, unsigned duration_ms);
	void write_volume(std::uint8_t value);

	void set_cpu_clock_hz(double hz) noexcept { if (hz > 0.0) m_cpu_hz = hz; }

	/// Record port \c F write; measures half-period from successive toggles of the warning bit (\c 0x02).
	void notify_mach_port_f(std::uint8_t prev_data, std::uint8_t next_data, std::uint64_t cpu_cycle, double cpu_hz);

	void set_handset_loopback(bool enable) noexcept { m_handset_loopback = enable; }
	void inject_handset_sample(float sample) noexcept { m_handset = sample; }

	/// Output sample at absolute time (seconds). Frequencies and cadence are applied here so tests
	/// measure the same signal the firmware would observe on the audio sink.
	float output_sample(double t_sec) const;

	millennium_alerter_board_config const &config() const noexcept { return m_cfg; }

	/// Cadence edge times in ms from cadence t0 (alternating tone_start / tone_end). Tables match `fixtures/audio/alerter-*.json`.
	static unsigned cadence_edge_count(millennium_alerter_cadence_kind k);
	static unsigned cadence_edge_ms(millennium_alerter_cadence_kind k, unsigned index);

private:
	float progress_tone_sample(double t_sec) const;
	float dtmf_sample(double t_sec) const;
	float mach_port_f_square_sample(double t_sec) const;
	static void dtmf_row_col_for_digit(std::uint8_t ascii, int &row_out, int &col_out);

	millennium_alerter_board_config m_cfg{};
	std::uint8_t m_tone = 0;
	std::uint8_t m_volume = 255;
	bool m_handset_loopback = false;
	float m_handset = 0.f;

	std::uint8_t m_dtmf_digit = '5';
	double m_dtmf_end_t = -1.0;
	double m_dtmf_start_t = 0.0;

	double m_cpu_hz = 12288000.0;
	std::uint8_t m_port_f_last_data = 0;
	std::uint64_t m_port_f_last_edge_cycle = 0;
	std::uint64_t m_port_f_half_period_cycles = 0;
	std::uint64_t m_port_f_phase_origin_cycle = 0;
	bool m_port_f_have_half_period = false;
};
