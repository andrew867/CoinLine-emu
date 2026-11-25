// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

enum class millennium_alerter_cadence_kind : std::uint8_t {
	none = 0,
	service_beep,
	error_beep,
	user_prompt_tone,
};

enum class millennium_enclosure_profile : std::uint8_t { ring_only, timer_warning, desk_oscillator };

struct millennium_alerter_board_config {
	unsigned sample_rate_hz = 8000;
	/// Optional clock metadata; audio timing uses actual emulated-time edges.
	double cpu_clock_hz = 0.0;
	millennium_enclosure_profile enclosure = millennium_enclosure_profile::timer_warning;
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
	void write_dtmf_digit(std::uint8_t ascii_digit, unsigned duration_ms, double now_sec = 0.0);
	void write_volume(std::uint8_t value);

	/// Record port F writes on the same emulated timebase as the audio stream.
	void notify_mach_port_f(std::uint8_t prev_data, std::uint8_t next_data, double event_time);
	void set_legacy_warning(bool selected) noexcept { m_legacy_warning = selected; }
	bool warning_selected() const noexcept;

	void set_handset_loopback(bool enable) noexcept { m_handset_loopback = enable; }
	void inject_handset_sample(float sample) noexcept { m_handset = sample; }
	void set_receive_muted(bool muted) noexcept { m_rx_muted = muted; }
	void set_local_dial_tone(bool enabled) noexcept { m_local_dial_tone = enabled; }
	bool local_dial_tone() const noexcept { return m_local_dial_tone; }
	void set_receive_gain(float gain) noexcept { m_rx_gain = gain; }

	/// High-fast alerter. Two half-duty pulses, 2000/3 Hz then 500 Hz, 50 ms each.
	/// The caller gates this with the firmware ringer enable and the ring-detect input.
	void prepare_high_fast_ringer(std::uint32_t sample_rate);
	void reset_high_fast_ringer() noexcept;
	float pull_high_fast_ringer() noexcept;

	/// Output sample at absolute time (seconds). Frequencies and cadence are applied here so tests
	/// measure the same signal the firmware would observe on the audio sink.
	float output_sample(double t_sec) const;
	/// On-hook ringer / warning square wave for the PC speakers.
	float alerter_speaker_sample(double t_sec) const;
	/// Off-hook earpiece: DTMF, call-progress tones, and the microphone sample.
	float handset_speaker_sample(double t_sec) const;

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
	bool m_rx_muted = false;
	bool m_local_dial_tone = false;
	float m_rx_gain = 1.f;
	float m_handset = 0.f;

	bool m_ringer_ready = false;
	std::uint32_t m_ring_phase = 0;
	std::uint32_t m_ring_elapsed = 0;
	std::uint32_t m_ring_steps[2]{};
	std::uint32_t m_ring_stage_ends[2]{};
	unsigned m_ring_stage = 0;
	std::int16_t m_ring_table[2][256]{};

	std::uint8_t m_dtmf_digit = '5';
	double m_dtmf_end_t = -1.0;
	double m_dtmf_start_t = 0.0;

	std::uint8_t m_port_f_last_data = 0;
	double m_port_f_last_edge_time = -1.0;
	double m_port_f_half_period = 0.0;
	bool m_legacy_warning = false;
};
