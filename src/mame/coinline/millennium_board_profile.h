// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>

#include "millennium_audio_model.h"
#include "millennium_coin_model.h"
#include "millennium_keypad_model.h"
#include "millennium_security_model.h"
#include "millennium_vfd_model.h"

struct millennium_z180_board_config {
	unsigned clock_hz = 12288000;
	int wait_rom = -1;
	int wait_ram = -1;
	int wait_io = -1;
};

bool millennium_board_parse_z180_profile(std::string const &board_json, millennium_z180_board_config &out,
	std::string &error_out);

bool millennium_board_parse_display_profile(std::string const &board_json, millennium_display_profile &out,
	std::string &error_out);

bool millennium_board_parse_keypad_profile(std::string const &board_json, millennium_keypad_board_config &out,
	std::string &error_out);

/// If `keypad.terminal_21_profile_id` was not set, infer profile from display + quick-access count (profile matrix harness rules).
void millennium_board_resolve_terminal21_profile(millennium_display_profile const &display, millennium_keypad_board_config &keypad);

bool millennium_board_parse_security_profile(std::string const &board_json, millennium_security_board_config &out,
	std::string &error_out);

bool millennium_board_parse_coin_profile(std::string const &board_json, millennium_coin_board_config &out,
	std::string &error_out);

bool millennium_board_parse_alerter_profile(std::string const &board_json, millennium_alerter_board_config &out,
	std::string &error_out);

struct millennium_memory_layout_config {
	std::uint32_t nvram_base = 0;
	std::uint32_t nvram_size = 0;
	std::uint32_t table_storage_base = 0;
	std::uint32_t table_storage_size = 0;
	std::uint32_t dla_stage_base = 0;
	std::uint32_t dla_stage_size = 0;
};

bool millennium_board_parse_memory_layout(std::string const &board_json, millennium_memory_layout_config &out,
	std::string &error_out);

/// terminal_23_user_io_feature_overlay_profiles — optional CP/runtime overlay flags for harness metadata (TP path unchanged).
struct millennium_user_io_overlay_traits {
	bool adsi_active = false;
	bool proton_active = false;
	bool mondex_active = false;
	bool git_ui_active = false;
	/// When true with dial/repeat TP→CP opcodes, drives `data_jack_model::manual_keypad_digit_signal` (terminal_09 spill path).
	bool data_jack_manual_keypad_active = false;
};

/// terminal_21 language_next_call_gate_matrix + terminal_24/25 — policy inputs for emulated CP routing of TP→CP bytes.
struct millennium_user_io_policy_config {
	/// When false, LANGUAGE / NEXT_CALL / guarded softkeys follow LANG_001 craft routing for user-if purposes.
	bool user_if_active = true;
	/// LANG_005: absorb language, next-call, softkeys (not dial unless protection_blocks_dial_pad).
	bool protection_blocks_user_if_soft_actions = false;
	/// ADSI activation refinement: with adsi_active, session=true participates in overlay block (LANG_004 style).
	bool adsi_runtime_session_active = false;
	/// Explicit LANG_004 overlay block for language/next-call when user_if off-hook.
	bool overlay_blocks_language_next_call = false;
	/// Per-overlay explicit LANG_004-style blocks (combined with overlay_traits at runtime).
	bool proton_ui_blocks_language_next_call = false;
	bool mondex_local_ui_blocks_language_next_call = false;
	bool git_ui_blocks_language_next_call = false;
	/// Optional: absorb dial/rep when protection_blocks_user_if_soft_actions-style policy arms dial path.
	bool protection_blocks_dial_pad = false;
	/// When true, opcodes classified as absorb_blocked are not queued to CP CSI/O (harness / strict policy runs).
	bool cp_absorb_blocked_user_if_opcodes = false;
};

struct millennium_user_io_board_config {
	millennium_user_io_overlay_traits overlay;
	millennium_user_io_policy_config policy;
};

bool millennium_board_parse_user_io_section(std::string const &board_json, millennium_user_io_board_config &out,
	std::string &error_out);
