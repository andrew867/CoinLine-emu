// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>

/// JSON Lines per `specs/audio-trace-format.spec.md` (schema `coinline.audio_trace/v1`).
/// Reads paths from `COINLINE_VOICEWARE_TRACE` / `COINLINE_AUDIO_TRACE` when set.
void millennium_audio_trace_append_voiceware(std::string const &line);
void millennium_audio_trace_append_superset(std::string const &line);
void millennium_audio_trace_emit_voiceware_row(std::string const &line);

/// Build one JSON object (no trailing newline) for a voiceware row.
std::string millennium_audio_trace_voiceware_json(std::uint64_t emulated_time_ns, std::uint64_t cycle, std::uint16_t pc,
	char const *event_type, std::uint16_t port, char rw, std::uint8_t value, unsigned bank, unsigned phrase,
	char const *compatibility_flag);

/// Rich phrase / status trace (`coinline.audio_trace/v1` + phrase correlation fields; no invented phrase text).
struct coinline_voiceware_phrase_trace {
	std::uint64_t emulated_time_ns = 0;
	std::uint64_t cycle = 0;
	std::uint16_t pc = 0;
	std::uint16_t sp = 0;
	char const *event_type = nullptr;
	std::uint16_t port = 0;
	char rw = 'w';
	std::uint8_t value = 0;
	unsigned bank_latch = 0;
	unsigned chip_bank = 0;
	std::uint8_t raw_sample_index = 0;
	char const *active_rom_id = nullptr;
	char const *active_blob = nullptr;
	char const *candidate_sample = nullptr;
	char const *candidate_text = nullptr;
	char const *mapping_confidence = nullptr;
	bool upd7759_idle_before = true;
	bool upd7759_idle_after = true;
	bool playback_started = false;
	bool playback_completed_edge = false;
	bool audio_non_silent_known = false;
	bool audio_non_silent = false;
	/// `legacy_software_decoder` / `upd7759_core` / `upd7759_core_fallback` when known.
	char const *execution_path = nullptr;
	char const *rom_sha256_u16 = nullptr;
	char const *rom_sha256_u26 = nullptr;
	char const *notes = nullptr;
};

std::string millennium_audio_trace_voiceware_phrase_detail_json(coinline_voiceware_phrase_trace const &t);

void millennium_audio_trace_emit_telephony_row(std::string const &line);
/// Route / conflict / `notify_voice` rows (also written to `COINLINE_AUDIO_ROUTE_TRACE` when set).
void millennium_audio_trace_emit_audio_route_path_row(std::string const &line);
/// Mute rows (also written to `COINLINE_MUTE_ROUTE_TRACE` when set).
void millennium_audio_trace_emit_mute_route_path_row(std::string const &line);

std::string millennium_audio_trace_telephony_raw_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint8_t value, char const *direction_tag);

std::string millennium_audio_trace_telephony_decode_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint8_t value, char const *command_label, char const *effect);

std::string millennium_audio_trace_telephony_unknown_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint8_t value);

std::string millennium_audio_trace_route_change_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::string const &prev_route, std::string const &new_route, std::string const &prev_mute_json,
	std::string const &new_mute_json, char const *notes, char const *call_state_if_known = nullptr);

std::string millennium_audio_trace_mute_change_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::string const &prev_mute_json, std::string const &new_mute_json,
	std::string const &route_context, char const *call_state_if_known = nullptr);

std::string millennium_audio_trace_route_conflict_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *route_snapshot, char const *compatibility_flag);

std::string millennium_audio_trace_telephony_processor_to_host_json(std::uint64_t emulated_time_ns,
	std::uint64_t cycle, std::uint16_t pc, std::uint8_t value);

void millennium_audio_trace_append_supervision(std::string const &line);
void millennium_audio_trace_emit_supervision_row(std::string const &line);

std::string millennium_audio_trace_supervision_status_code_json(std::uint64_t emulated_time_ns,
	std::uint64_t cycle, std::uint16_t pc, std::uint8_t status_code);

std::string millennium_audio_trace_disconnect_event_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *disconnect_event_enum);

std::string millennium_audio_trace_supervision_timeout_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc);

void millennium_audio_trace_append_alerter(std::string const &line);
void millennium_audio_trace_emit_alerter_row(std::string const &line);

std::string millennium_audio_trace_alerter_ready_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc);

std::string millennium_audio_trace_alerter_gpio_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint16_t port_hex, std::uint8_t value);

std::string millennium_audio_trace_alerter_tone_start_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *tone_class, unsigned edge_ms, unsigned edge_index);

std::string millennium_audio_trace_alerter_tone_end_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *tone_class, unsigned edge_ms, unsigned edge_index);
