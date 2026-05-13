// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_board_profile.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class millennium_nvram_model {
public:
	void configure(millennium_memory_layout_config const &layout);

	void reset_session();

	bool load_envelope_json(std::string const &json_text, std::string &error_out);
	bool save_envelope_json(std::string &json_out, std::string &error_out) const;

	std::uint8_t read_nvram(std::uint32_t offset) const;
	bool write_nvram(std::uint32_t offset, std::uint8_t value, std::string &error_out);

	std::uint8_t read_table(std::uint32_t offset) const;
	bool write_table(std::uint32_t offset, std::uint8_t value, std::string &error_out);

	std::uint8_t read_dla(std::uint32_t offset) const;
	bool write_dla(std::uint32_t offset, std::uint8_t value, std::string &error_out);

	bool verify_checksum(std::string &error_out) const;
	bool checksum_failure() const noexcept { return m_checksum_failure; }
	void recover_default_cleared(std::string &error_out);

	bool apply_dla_to_flash(std::vector<std::uint8_t> &flash_rom, std::string &error_out);

	std::vector<std::string> const &access_log() const noexcept { return m_log; }

	bool serialize_state(std::vector<std::uint8_t> &out) const;
	bool deserialize_state(std::vector<std::uint8_t> const &in, std::string &error_out);

	static std::uint8_t compute_sum8(std::vector<std::uint8_t> const &data);
	static bool parse_hex_u8(std::string const &hex, std::uint8_t &out);

private:
	void pad_vectors();

	millennium_memory_layout_config m_layout{};
	std::vector<std::uint8_t> m_nvram{};
	std::vector<std::uint8_t> m_table{};
	std::vector<std::uint8_t> m_dla{};
	bool m_checksum_failure = false;
	bool m_use_sum8_envelope = false;
	std::uint8_t m_envelope_sum8 = 0;
	std::vector<std::string> m_log{};
};
