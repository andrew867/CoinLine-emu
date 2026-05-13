// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "millennium_am8048_core.h"
#include "millennium_keypad_model.h"
#include "millennium_pcd3349a_contract.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class millennium_pcd3349a
{
public:
	enum class tone_mode : std::uint8_t { none, dialtone, nis };

	struct front_panel_result {
		std::vector<std::uint8_t> tp_events;
		bool hook_changed = false;
		bool hook_onhook = true;
	};

	void reset(bool onhook_initial = true);
	void step(std::uint64_t cycles) noexcept { m_core.step_cycles(cycles); }
	void set_cp_hz(std::uint32_t cp_hz) noexcept { m_cp_hz = cp_hz; }
	void set_trace_paths(std::filesystem::path runtime, std::filesystem::path port, std::filesystem::path keypad,
		std::filesystem::path tone, std::filesystem::path cp_protocol);
	char const *backend_id_string() const noexcept { return m_tp.backend_id_string(); }
	std::uint32_t xtal_hz() const noexcept { return m_tp_xtal_hz; }

	front_panel_result process_front_panel(std::uint32_t keymatrix, std::uint32_t linectrl, std::uint32_t softkeys_raw,
		std::uint8_t secmask, bool oos_visible, bool voice_active, millennium_terminal21_user_io_profile profile,
		std::uint64_t cycle, std::uint64_t hz);

	std::vector<std::uint8_t> handle_cp_to_tp_byte(std::uint8_t byte, bool hook_onhook_stable);

	tone_mode compute_tone_mode(bool off_hook, bool rx_open, bool voice_active, bool oos_mode) const noexcept;

private:
	millennium_am8048_core m_core;
	coinline::tp8048::pcd3349a_contract m_tp{ m_core };
	std::uint32_t m_last_keymatrix = 0U;
	std::uint32_t m_last_linectrl = 0U;
	std::uint32_t m_last_softkeys = 0U;
	std::uint64_t m_hook_last_transition_cycle = 0ULL;
	bool m_hook_onhook = true;
	std::uint32_t m_cp_hz = 1U;
	std::uint32_t m_tp_xtal_hz = 3579545U;
	std::filesystem::path m_runtime_trace_path;
	std::filesystem::path m_port_trace_path;
	std::filesystem::path m_keypad_trace_path;
	std::filesystem::path m_tone_trace_path;
	std::filesystem::path m_cp_protocol_trace_path;
	bool m_rom_loaded = false;
	std::string m_rom_source = "placeholder";

	void append_trace(std::filesystem::path const &path, char const *event, std::uint64_t cp_cycle, std::uint8_t value) const;
};
