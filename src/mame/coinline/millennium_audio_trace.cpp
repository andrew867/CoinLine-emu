// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_audio_trace.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

std::string json_escape_cstr(char const *r)
{
	if (!r || !*r)
		return std::string();
	std::string o;
	for (char const *p = r; *p; ++p) {
		if (*p == '\\' || *p == '"')
			o.push_back('\\');
		o.push_back(*p);
	}
	return o;
}

void append_file(std::filesystem::path const &p, std::string const &line)
{
	if (p.empty())
		return;
	std::FILE *f = std::fopen(p.string().c_str(), "ab");
	if (!f)
		return;
	if (!line.empty())
		std::fwrite(line.data(), 1, line.size(), f);
	std::fputc('\n', f);
	std::fclose(f);
}

std::filesystem::path path_from_env(char const *name)
{
	if (!name)
		return {};
	char const *p = std::getenv(name);
	if (!p || !*p)
		return {};
	return std::filesystem::path(p);
}

} // namespace

void millennium_audio_trace_append_voiceware(std::string const &line) { append_file(path_from_env("COINLINE_VOICEWARE_TRACE"), line); }

void millennium_audio_trace_append_superset(std::string const &line) { append_file(path_from_env("COINLINE_AUDIO_TRACE"), line); }

void millennium_audio_trace_append_alerter(std::string const &line)
{
	append_file(path_from_env("COINLINE_ALERTER_TRACE"), line);
}

void millennium_audio_trace_emit_alerter_row(std::string const &line)
{
	millennium_audio_trace_append_alerter(line);
	millennium_audio_trace_append_superset(line);
}

void millennium_audio_trace_emit_voiceware_row(std::string const &line)
{
	millennium_audio_trace_append_voiceware(line);
	millennium_audio_trace_append_superset(line);
}

std::string millennium_audio_trace_voiceware_phrase_detail_json(coinline_voiceware_phrase_trace const &t)
{
	// MAME `upd7759_device::busy_r` is true when the core is in the idle/stop state; "playing" ~= !idle.
	bool const playing_before = !t.upd7759_idle_before;
	bool const playing_after = !t.upd7759_idle_after;
	char phex[12], sphex[12], prthex[12], vhex[12], rawhex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(t.pc));
	std::snprintf(sphex, sizeof(sphex), "\"0x%04X\"", unsigned(t.sp));
	std::snprintf(prthex, sizeof(prthex), "\"0x%04X\"", unsigned(t.port));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(t.value));
	std::snprintf(rawhex, sizeof(rawhex), "\"0x%02X\"", unsigned(t.raw_sample_index));
	char const *ev = t.event_type && *t.event_type ? t.event_type : "unknown";
	char const *rom = t.active_rom_id && *t.active_rom_id ? t.active_rom_id : "";
	char const *blob = t.active_blob && *t.active_blob ? t.active_blob : "";
	char const *conf = t.mapping_confidence && *t.mapping_confidence ? t.mapping_confidence : "unknown";
	char const *notes = t.notes && *t.notes ? t.notes : "";
	std::ostringstream o;
	o << "{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":" << t.emulated_time_ns
	  << ",\"cycle\":" << t.cycle << ",\"pc\":" << phex << ",\"sp\":" << sphex << ",\"event_type\":\"" << ev
	  << "\",\"device\":\"voiceware\",\"port\":" << prthex << ",\"direction\":\"" << (t.rw == 'r' ? "read" : "write")
	  << "\",\"value\":" << vhex << ",\"command\":" << vhex << ",\"raw_sample_index\":" << rawhex
	  << ",\"bank_latch\":" << unsigned(t.bank_latch) << ",\"active_bank\":" << unsigned(t.chip_bank)
	  << ",\"active_rom\":\"" << rom << "\",\"active_blob\":\"" << blob << "\"";
	if (t.candidate_sample && *t.candidate_sample)
		o << ",\"candidate_sample\":\"" << json_escape_cstr(t.candidate_sample) << '"';
	else
		o << ",\"candidate_sample\":null";
	if (t.candidate_text && *t.candidate_text)
		o << ",\"candidate_text\":\"" << json_escape_cstr(t.candidate_text) << '"';
	else
		o << ",\"candidate_text\":null";
	o << ",\"mapping_confidence\":\"" << json_escape_cstr(conf) << '"';
	o << ",\"upd7759_idle_before\":" << (t.upd7759_idle_before ? "true" : "false")
	  << ",\"upd7759_idle_after\":" << (t.upd7759_idle_after ? "true" : "false")
	  << ",\"upd7759_playing_before\":" << (playing_before ? "true" : "false")
	  << ",\"upd7759_playing_after\":" << (playing_after ? "true" : "false")
	  << ",\"playback_started\":" << (t.playback_started ? "true" : "false")
	  << ",\"playback_completed_edge\":" << (t.playback_completed_edge ? "true" : "false")
	  << ",\"audio_non_silent_known\":" << (t.audio_non_silent_known ? "true" : "false")
	  << ",\"audio_non_silent\":" << (t.audio_non_silent ? "true" : "false");
	if (t.execution_path && *t.execution_path)
		o << ",\"execution_path\":\"" << json_escape_cstr(t.execution_path) << '"';
	else
		o << ",\"execution_path\":null";
	o << ",\"upd7759_busy_before\":" << (t.upd7759_idle_before ? "false" : "true")
	  << ",\"upd7759_busy_after\":" << (t.upd7759_idle_after ? "false" : "true");
	if (t.rom_sha256_u16 && *t.rom_sha256_u16)
		o << ",\"rom_sha256_u16\":\"" << json_escape_cstr(t.rom_sha256_u16) << '"';
	else
		o << ",\"rom_sha256_u16\":null";
	if (t.rom_sha256_u26 && *t.rom_sha256_u26)
		o << ",\"rom_sha256_u26\":\"" << json_escape_cstr(t.rom_sha256_u26) << '"';
	else
		o << ",\"rom_sha256_u26\":null";
	o << ",\"notes\":\"" << json_escape_cstr(notes) << "\"}";
	return o.str();
}

std::string millennium_audio_trace_voiceware_json(std::uint64_t emulated_time_ns, std::uint64_t cycle, std::uint16_t pc,
	char const *event_type, std::uint16_t port, char rw, std::uint8_t value, unsigned bank, unsigned phrase,
	char const *compatibility_flag)
{
	char buf[900];
	char const *ev = event_type && *event_type ? event_type : "unknown";
	char phex[12], prthex[12], vhex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(prthex, sizeof(prthex), "\"0x%04X\"", unsigned(port));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(value));
	char const *dir = (rw == 'r') ? "\"read\"" : "\"write\"";
	char const *cf = (compatibility_flag && *compatibility_flag) ? compatibility_flag : nullptr;
	if (cf)
		std::snprintf(buf, sizeof(buf),
			"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
			"\"pc\":%s,\"event_type\":\"%s\",\"device\":\"voiceware\",\"port\":%s,\"direction\":%s,"
			"\"value\":%s,\"prompt_id\":{\"bank\":%u,\"phrase\":%u},\"compatibility_flag\":\"%s\"}",
			static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, ev, prthex,
			dir, vhex, bank, phrase, cf);
	else
		std::snprintf(buf, sizeof(buf),
			"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
			"\"pc\":%s,\"event_type\":\"%s\",\"device\":\"voiceware\",\"port\":%s,\"direction\":%s,"
			"\"value\":%s,\"prompt_id\":{\"bank\":%u,\"phrase\":%u}}",
			static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, ev, prthex,
			dir, vhex, bank, phrase);
	return std::string(buf);
}

void millennium_audio_trace_emit_telephony_row(std::string const &line)
{
	append_file(path_from_env("COINLINE_TELEPHONY_TRACE"), line);
	millennium_audio_trace_append_superset(line);
}

void millennium_audio_trace_emit_audio_route_path_row(std::string const &line)
{
	append_file(path_from_env("COINLINE_AUDIO_ROUTE_TRACE"), line);
	millennium_audio_trace_append_superset(line);
}

void millennium_audio_trace_emit_mute_route_path_row(std::string const &line)
{
	append_file(path_from_env("COINLINE_MUTE_ROUTE_TRACE"), line);
	millennium_audio_trace_append_superset(line);
}

std::string millennium_audio_trace_telephony_raw_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint8_t value, char const *direction_tag)
{
	char buf[480];
	char phex[12], vhex[12], nbuf[64];
	char const *n = (direction_tag && *direction_tag) ? direction_tag : "host_to_processor";
	std::snprintf(nbuf, sizeof(nbuf), "\"%s\"", n);
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(value));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"telephony_host_to_processor_byte\",\"device\":\"telephony\","
		"\"value\":%s,\"notes\":%s}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, vhex, nbuf);
	return std::string(buf);
}

std::string millennium_audio_trace_telephony_decode_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint8_t value, char const *command_label, char const *effect)
{
	char buf[640];
	char phex[12], vhex[12], cmdhex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(value));
	std::snprintf(cmdhex, sizeof(cmdhex), "\"0x%02X\"", unsigned(value));
	char const *cl = command_label && *command_label ? command_label : "";
	char const *ef = effect && *effect ? effect : "";
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"telephony_command_decode\",\"device\":\"telephony\",\"value\":%s,"
		"\"decoded_command\":{\"hex\":%s,\"label\":\"%s\",\"effect\":\"%s\"}}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, vhex, cmdhex, cl, ef);
	return std::string(buf);
}

std::string millennium_audio_trace_telephony_unknown_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint8_t value)
{
	char buf[420];
	char phex[12], vhex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(value));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"telephony_host_to_processor_byte\",\"device\":\"telephony\","
		"\"value\":%s,\"notes\":\"unmapped_routing_command\",\"compatibility_flag\":\"compatibility_validation_required\"}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, vhex);
	return std::string(buf);
}

std::string millennium_audio_trace_route_change_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::string const &prev_route, std::string const &new_route, std::string const &prev_mute_json,
	std::string const &new_mute_json, char const *notes, char const *call_state_if_known)
{
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	char const *r = notes && *notes ? notes : "";
	std::string const notes_esc = json_escape_cstr(r);
	std::ostringstream o;
	o << "{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":"
	  << static_cast<unsigned long long>(emulated_time_ns) << ",\"cycle\":"
	  << static_cast<unsigned long long>(cycle) << ",\"pc\":" << phex
	  << ",\"event_type\":\"route_change\",\"device\":\"audio_route\","
	     "\"previous_state\":{\"route_state\":\""
	  << prev_route << "\",\"mute_state\":" << prev_mute_json << "},\"new_state\":{\"route_state\":\"" << new_route
	  << "\",\"mute_state\":" << new_mute_json << "},\"route_state\":\"" << new_route << "\",\"notes\":\"" << notes_esc
	  << '"';
	if (call_state_if_known && *call_state_if_known)
		o << ",\"call_state_if_known\":\"" << call_state_if_known << '"';
	o << '}';
	return o.str();
}

std::string millennium_audio_trace_mute_change_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::string const &prev_mute_json, std::string const &new_mute_json,
	std::string const &route_context, char const *call_state_if_known)
{
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::ostringstream o;
	o << "{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":"
	  << static_cast<unsigned long long>(emulated_time_ns) << ",\"cycle\":"
	  << static_cast<unsigned long long>(cycle) << ",\"pc\":" << phex
	  << ",\"event_type\":\"mute_change\",\"device\":\"audio_route\","
	     "\"previous_state\":{\"mute_state\":"
	  << prev_mute_json << "},\"new_state\":{\"mute_state\":" << new_mute_json << "},\"route_state\":\"" << route_context
	  << '"';
	if (call_state_if_known && *call_state_if_known)
		o << ",\"call_state_if_known\":\"" << call_state_if_known << '"';
	o << '}';
	return o.str();
}

std::string millennium_audio_trace_route_conflict_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *route_snapshot, char const *compatibility_flag)
{
	char buf[520];
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	char const *rs = route_snapshot && *route_snapshot ? route_snapshot : "";
	char const *cf = compatibility_flag && *compatibility_flag ? compatibility_flag : "";
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"route_conflict_resolved\",\"device\":\"audio_route\","
		"\"route_state\":\"%s\",\"compatibility_flag\":\"%s\"}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, rs, cf);
	return std::string(buf);
}

std::string millennium_audio_trace_alerter_ready_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc)
{
	char buf[400];
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"alerter_ready\",\"device\":\"audio\"}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex);
	return std::string(buf);
}

std::string millennium_audio_trace_alerter_gpio_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, std::uint16_t port_hex, std::uint8_t value)
{
	char buf[450];
	char phex[12], porth[12], vhex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(porth, sizeof(porth), "\"0x%04X\"", unsigned(port_hex));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(value));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"alerter_gpio_write\",\"device\":\"audio\",\"port\":%s,\"direction\":"
		"\"write\",\"value\":%s}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, porth, vhex);
	return std::string(buf);
}

static std::string alerter_tone_json(std::uint64_t emulated_time_ns, std::uint64_t cycle, std::uint16_t pc,
	char const *event_type, char const *tone_class, unsigned edge_ms, unsigned edge_index)
{
	char buf[520];
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	char const *ev = event_type && *event_type ? event_type : "unknown";
	char const *tcl = tone_class && *tone_class ? tone_class : "unknown";
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"%s\",\"device\":\"audio\",\"tone_class\":\"%s\","
		"\"edge_ms\":%u,\"edge_index\":%u}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, ev, tcl,
		unsigned(edge_ms), unsigned(edge_index));
	return std::string(buf);
}

std::string millennium_audio_trace_alerter_tone_start_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *tone_class, unsigned edge_ms, unsigned edge_index)
{
	return alerter_tone_json(emulated_time_ns, cycle, pc, "alerter_tone_start", tone_class, edge_ms, edge_index);
}

std::string millennium_audio_trace_alerter_tone_end_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *tone_class, unsigned edge_ms, unsigned edge_index)
{
	return alerter_tone_json(emulated_time_ns, cycle, pc, "alerter_tone_end", tone_class, edge_ms, edge_index);
}

void millennium_audio_trace_append_supervision(std::string const &line)
{
	append_file(path_from_env("COINLINE_SUPERVISION_TRACE"), line);
}

void millennium_audio_trace_emit_supervision_row(std::string const &line)
{
	millennium_audio_trace_append_supervision(line);
	millennium_audio_trace_append_superset(line);
}

std::string millennium_audio_trace_telephony_processor_to_host_json(std::uint64_t emulated_time_ns,
	std::uint64_t cycle, std::uint16_t pc, std::uint8_t value)
{
	char buf[480];
	char phex[12], vhex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(vhex, sizeof(vhex), "\"0x%02X\"", unsigned(value));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"telephony_processor_to_host_byte\",\"device\":\"telephony\",\"value\":%s}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, vhex);
	return std::string(buf);
}

std::string millennium_audio_trace_supervision_status_code_json(std::uint64_t emulated_time_ns,
	std::uint64_t cycle, std::uint16_t pc, std::uint8_t status_code)
{
	char buf[520];
	char phex[12], shex[16];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(shex, sizeof(shex), "\"0x%02X\"", unsigned(status_code));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"supervision_status_code\",\"device\":\"supervision\",\"status_code_hex\":%s,"
		"\"compatibility_flag\":\"compatibility_validation_required\"}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, shex);
	return std::string(buf);
}

std::string millennium_audio_trace_disconnect_event_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc, char const *disconnect_event_enum)
{
	char buf[560];
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	char const *d = disconnect_event_enum && *disconnect_event_enum ? disconnect_event_enum : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"disconnect_event\",\"device\":\"supervision\",\"disconnect_event\":\"%s\","
		"\"compatibility_flag\":\"compatibility_validation_required\"}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex, d);
	return std::string(buf);
}

std::string millennium_audio_trace_supervision_timeout_json(std::uint64_t emulated_time_ns, std::uint64_t cycle,
	std::uint16_t pc)
{
	char buf[480];
	char phex[12];
	std::snprintf(phex, sizeof(phex), "\"0x%04X\"", unsigned(pc));
	std::snprintf(buf, sizeof(buf),
		"{\"schema_version\":\"coinline.audio_trace/v1\",\"timestamp_emulated_ns\":%llu,\"cycle\":%llu,"
		"\"pc\":%s,\"event_type\":\"supervision_timeout\",\"device\":\"supervision\","
		"\"compatibility_flag\":\"compatibility_validation_required\"}",
		static_cast<unsigned long long>(emulated_time_ns), static_cast<unsigned long long>(cycle), phex);
	return std::string(buf);
}
