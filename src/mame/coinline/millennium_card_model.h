// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct millennium_mag_fixture {
	unsigned track = 2;
	std::string payload_track_ascii;
	unsigned swipe_duration_ms = 350;
	unsigned bit_rate_bps = 210;
	bool force_lrc_error = false;
	bool force_parity_error = false;
};

class millennium_card_model {
public:
	enum class track2_decode_error : std::uint8_t {
		none = 0,
		missing_start_sentinel,
		missing_end_sentinel,
		illegal_character,
		empty_pan,
		parity_failed,
		lrc_failed,
	};

	struct track2_decode_result {
		track2_decode_error error = track2_decode_error::none;
		std::string pan;
		std::string body;
		bool reverse_swipe = false;
	};

	bool parse_fixture_json(std::string const &json_text, std::string &error_out);

	void reset_session();

	void arm_swipe(std::uint64_t start_cycle, std::uint64_t cpu_hz);
	void abort_swipe();

	static std::uint8_t xor_lrc_byte(std::string_view body_ascii);
	static bool track_xor_ok(std::string_view full_track_ascii);
	static track2_decode_result decode_track2_ascii(std::string_view full_track_ascii, bool require_lrc,
		bool force_parity_error = false);

	std::uint8_t status_bits(std::uint64_t cycle) const;
	std::uint8_t data_byte(std::uint64_t cycle) const;

	/// Install / self-test paths may poll for stuck card-path sensors before prompting insertion.
	void set_path_sensors_stuck(bool cp_stuck, bool cfs_stuck) noexcept;

	std::uint64_t cycles_per_bit(std::uint64_t cpu_hz) const;
	std::size_t bit_count() const noexcept { return m_bits.size(); }
	bool lrc_ok() const noexcept { return m_lrc_ok; }

	bool swipe_active(std::uint64_t cycle) const;

private:
	void rebuild_bits_from_track();

	millennium_mag_fixture m_fix{};
	std::vector<std::uint8_t> m_bits{};
	bool m_lrc_ok = false;
	std::uint64_t m_swipe_start_cycle = 0;
	bool m_armed = false;
	std::uint64_t m_cpu_hz_arm = 1;
	bool m_sensor_cp_stuck = false;
	bool m_sensor_cfs_stuck = false;
};
