// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_voiceware.h"

#include "millennium_audio_trace.h"
#include "millennium_sha256.h"
#include "millennium_voiceware_config.h"
#include "millennium_voiceware_phrase_lookup.h"

#include "emu.h"

#include <fstream>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>

DEFINE_DEVICE_TYPE(MILLENNIUM_VOICEWARE, millennium_voiceware_device, "millennium_voiceware",
	"CoinLine Millennium voiceware (uPD7759)")

namespace {

constexpr std::array<std::array<int, 16>, 16> k_upd7759_step = {{
	{{ 0, 0, 1, 2, 3, 5, 7, 10, 0, 0, -1, -2, -3, -5, -7, -10 }},
	{{ 0, 1, 2, 3, 4, 6, 8, 13, 0, -1, -2, -3, -4, -6, -8, -13 }},
	{{ 0, 1, 2, 4, 5, 7, 10, 15, 0, -1, -2, -4, -5, -7, -10, -15 }},
	{{ 0, 1, 3, 4, 6, 9, 13, 19, 0, -1, -3, -4, -6, -9, -13, -19 }},
	{{ 0, 2, 3, 5, 8, 11, 15, 23, 0, -2, -3, -5, -8, -11, -15, -23 }},
	{{ 0, 2, 4, 7, 10, 14, 19, 29, 0, -2, -4, -7, -10, -14, -19, -29 }},
	{{ 0, 3, 5, 8, 12, 16, 22, 33, 0, -3, -5, -8, -12, -16, -22, -33 }},
	{{ 1, 4, 7, 10, 15, 20, 29, 43, -1, -4, -7, -10, -15, -20, -29, -43 }},
	{{ 1, 4, 8, 13, 18, 25, 35, 53, -1, -4, -8, -13, -18, -25, -35, -53 }},
	{{ 1, 6, 10, 16, 22, 31, 43, 64, -1, -6, -10, -16, -22, -31, -43, -64 }},
	{{ 2, 7, 12, 19, 27, 37, 51, 76, -2, -7, -12, -19, -27, -37, -51, -76 }},
	{{ 2, 9, 16, 24, 34, 46, 64, 96, -2, -9, -16, -24, -34, -46, -64, -96 }},
	{{ 3, 11, 19, 29, 41, 57, 79, 117, -3, -11, -19, -29, -41, -57, -79, -117 }},
	{{ 4, 13, 24, 36, 50, 69, 96, 143, -4, -13, -24, -36, -50, -69, -96, -143 }},
	{{ 4, 16, 29, 44, 62, 85, 118, 175, -4, -16, -29, -44, -62, -85, -118, -175 }},
	{{ 6, 20, 36, 54, 76, 104, 144, 214, -6, -20, -36, -54, -76, -104, -144, -214 }},
}};

constexpr std::array<int, 16> k_upd7759_state = {{ -1, -1, 0, 0, 1, 2, 2, 3, -1, -1, 0, 0, 1, 2, 2, 3 }};

char const k_voiceware_exec_legacy[] = "legacy_software_decoder";
char const k_voiceware_exec_core[] = "upd7759_core";
char const k_voiceware_exec_core_fallback[] = "upd7759_core_fallback";

void write_text_file_if_env(char const *env_name, std::string const &body)
{
	if (!env_name)
		return;
	char const *p = osd_getenv(env_name);
	if (!p || !*p)
		return;
	std::ofstream f(p, std::ios::binary);
	if (f)
		f << body;
}

void append_line_if_env(char const *env_name, std::string const &line)
{
	char const *p = osd_getenv(env_name);
	std::filesystem::path path;
	if (p && *p) {
		path = p;
	} else if (std::string(env_name) == "COINLINE_VOICEWARE_DECODE_TRACE") {
		char const *voiceware_trace = osd_getenv("COINLINE_VOICEWARE_TRACE");
		if (!voiceware_trace || !*voiceware_trace)
			return;
		path = std::filesystem::path(voiceware_trace).parent_path() / "voiceware-decode-trace.jsonl";
	} else {
		return;
	}
	std::ofstream f(path, std::ios::binary | std::ios::app);
	if (f)
		f << line << '\n';
}

std::string hex_byte(u8 value)
{
	char buf[8];
	std::snprintf(buf, sizeof(buf), "0x%02X", unsigned(value));
	return buf;
}

std::string hex_offset(u32 value)
{
	char buf[12];
	std::snprintf(buf, sizeof(buf), "0x%06X", unsigned(value));
	return buf;
}

std::int16_t pcm_from_predictor(int sample)
{
	return std::int16_t(std::clamp(sample * 128, -32768, 32767));
}

u32 rate_from_header(u8 op)
{
	// 160 kHz is the uPD7759 timing base (640 kHz / 4), not the host PCM stream rate.
	// The block divider yields the native ADPCM cadence; `append_linear_resampled` maps it to 48 kHz.
	unsigned const divider = unsigned(op & 0x3fU) + 1U;
	return std::max<u32>(1U, 160000U / divider);
}

enum class decode_mode {
	spec,
	reference,
};

decode_mode decode_mode_from_env()
{
	char const *mode = osd_getenv("COINLINE_VOICEWARE_DECODER_MODE");
	if (!mode || !*mode)
		return decode_mode::reference;
	std::string const v(mode);
	if (v == "spec")
		return decode_mode::spec;
	return decode_mode::reference;
}

void append_linear_resampled(std::vector<std::int16_t> &dst, std::vector<std::int16_t> const &src, u32 src_rate)
{
	if (src.empty() || src_rate == 0U)
		return;
	u32 constexpr dst_rate = 48000U;
	std::size_t const count = std::max<std::size_t>(1U,
		std::size_t(std::llround(double(src.size()) * double(dst_rate) / double(src_rate))));
	for (std::size_t i = 0; i < count; ++i) {
		double const pos = double(i) * double(src_rate) / double(dst_rate);
		std::size_t const base = std::min<std::size_t>(std::size_t(pos), src.size() - 1U);
		std::size_t const next = std::min<std::size_t>(base + 1U, src.size() - 1U);
		double const frac = pos - double(base);
		double const y = double(src[base]) * (1.0 - frac) + double(src[next]) * frac;
		dst.push_back(std::int16_t(std::clamp<int>(int(std::lround(y)), -32768, 32767)));
	}
}

} // namespace

millennium_voiceware_device::millennium_voiceware_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_VOICEWARE, tag, owner, clock)
	, device_sound_interface(mconfig, *this)
	, m_upd(*this, "^:voicew_upd")
	, m_audio_route(*this, "^:audroute")
	, m_z180(*this, "^:maincpu")
	, m_vwspk(*this, "^:vwspk")
{
}

void millennium_voiceware_device::device_start()
{
	m_stream = stream_alloc(0, 1, 48000);
	// `required_device<upd7759_device>(^:voicew_upd)` would have failed construction if the tag were wrong.
	memory_region *const vr = memregion(":voicew");
	u32 const rom_bytes = vr ? u32(vr->bytes()) : 0U;
	bool const spk = m_vwspk;
	bool const core_env = coinline_voiceware_upd7759_core_from_env();
	std::string sha_u16, sha_u26;
	if (vr && vr->base() && rom_bytes >= 0x200000U) {
		sha_u16 = millennium_sha256_hex(vr->base(), 0x100000U);
		sha_u26 = millennium_sha256_hex(vr->base() + 0x100000U, 0x100000U);
	}
	coinline_voiceware_phrase_port_levels_from_osd(m_phrase_port_levels);
	m_retrigger_policy = coinline_voiceware_retrigger_policy_from_env();
	auto const phex = [](u8 b) {
		char buf[8];
		std::snprintf(buf, sizeof(buf), "0x%02X", unsigned(b));
		return std::string(buf);
	};
	char const *const rpol = (m_retrigger_policy == coinline_voiceware_retrigger_policy::allow_duplicate_strobe)
		? "allow"
		: "suppress";
	std::ostringstream d;
	d << "{"
		 "\"schema_version\":\"coinline.voiceware_startup/v1\","
		 "\"voicew_upd_finder_resolved\":true,"
		 "\"voicew_upd_tag\":\"voicew_upd\","
		 "\"memory_region_voicew_bytes\":" << unsigned(rom_bytes) << ","
		 "\"voicew_region_present\":" << (vr ? "true" : "false") << ","
		 "\"speaker_vwspk_present\":" << (spk ? "true" : "false") << ","
		 "\"upd7759_core_env\":" << (core_env ? "true" : "false") << ","
		 "\"phrase_0x61_idle\":\"" << phex(m_phrase_port_levels.idle_read) << "\","
		 "\"phrase_0x61_busy\":\"" << phex(m_phrase_port_levels.busy_read) << "\","
		 "\"phrase_0x61_fault\":\"" << phex(m_phrase_port_levels.fault_read) << "\","
		 "\"retrigger_policy\":\"" << rpol << "\","
		 "\"voice_rom_u16_sha256\":"
		 << (sha_u16.empty() ? "null" : std::string("\"") + sha_u16 + "\"")
		 << ",\"voice_rom_u26_sha256\":"
		 << (sha_u26.empty() ? "null" : std::string("\"") + sha_u26 + "\"") << ","
		 "\"notes_mame_upd7759\":\"required_device<upd7759> for ^:voicew_upd resolved; "
		 "MAME busy_r is true when the uPD core is idle. "
		 "0x61 read bytes are env-configurable (COINLINE_VOICEWARE_0x61_*).\""
		 "}";
	std::string const json = d.str();
	write_text_file_if_env("COINLINE_VOICEWARE_STARTUP_JSON", json);
	// Also mirror into the voiceware JSONL when enabled (evidence sidecar).
	if (char const *vwt = osd_getenv("COINLINE_VOICEWARE_TRACE")) {
		if (vwt && *vwt) {
			std::string const line = std::string("{\"schema_version\":\"coinline.voiceware_startup/v1\","
				 "\"event_type\":\"voiceware_device_start\",\"device\":\"voiceware\","
				 "\"voicew_upd_resolved\":true,\"rom_region_voicew_bytes\":") +
				std::to_string(rom_bytes) + ",\"speaker_route_vwspk\":" + (spk ? "true" : "false") + "}";
			millennium_audio_trace_emit_voiceware_row(line);
		}
	}
	if (sha_u16.empty() || sha_u26.empty())
		logerror("coinline_voiceware: ^:voicew_upd ok; :voicew %u bytes; vwspk %s; voice ROM SHA-256 unavailable "
			 "(need full 2 MiB region)\n",
			unsigned(rom_bytes), spk ? "ok" : "absent");
	else
		logerror("coinline_voiceware: ^:voicew_upd ok; :voicew %u bytes; vwspk %s; U16 sha256=%.16s… U26 sha256=%.16s… "
			 "upd7759_core_env=%d; 0x61 idle/busy/fault=%02X/%02X/%02X; retrigger=%s\n",
			unsigned(rom_bytes), spk ? "ok" : "absent", sha_u16.c_str(), sha_u26.c_str(), int(core_env),
			unsigned(m_phrase_port_levels.idle_read), unsigned(m_phrase_port_levels.busy_read),
			unsigned(m_phrase_port_levels.fault_read), rpol);
	m_upd7759_core_env = core_env;
	m_voice_rom_sha256_u16 = std::move(sha_u16);
	m_voice_rom_sha256_u26 = std::move(sha_u26);
	save_item(NAME(m_reset_asserted));
	save_item(NAME(m_bank_latched));
	save_item(NAME(m_bank));
	save_item(NAME(m_last_phrase));
	save_item(NAME(m_last_chip_bank));
	save_item(NAME(m_phrase_pos));
	save_item(NAME(m_decoder_playing));
	save_item(NAME(m_had_playback));
	save_item(NAME(m_upd7759_core_env));
}

void millennium_voiceware_device::device_reset()
{
	device_t::device_reset();
	m_reset_asserted = false;
	m_bank_latched = false;
	m_bank = 0;
	m_last_phrase = 0;
	m_last_chip_bank = 0xffU;
	m_had_playback = false;
	m_phrase_pcm.clear();
	m_phrase_pos = 0;
	m_decoder_playing = false;
	if (m_stream)
		m_stream->update();
}

void millennium_voiceware_device::sound_stream_update(sound_stream &stream)
{
	if (m_upd7759_core_env) {
		for (int i = 0; i < stream.samples(); ++i)
			stream.put_int(0, i, 0, 32768);
		return;
	}
	for (int i = 0; i < stream.samples(); ++i) {
		if (m_phrase_pos < m_phrase_pcm.size()) {
			stream.put_int(0, i, m_phrase_pcm[m_phrase_pos++], 32768);
		} else {
			stream.put_int(0, i, 0, 32768);
			m_decoder_playing = false;
		}
	}
}

std::uint64_t millennium_voiceware_device::emulated_ns() const
{
	double const sec = machine().time().as_double();
	return static_cast<std::uint64_t>(sec * 1e9);
}

void millennium_voiceware_device::emit_row(char const *event_type, std::uint64_t cpu_cycle, std::uint16_t pc,
	std::uint16_t port, char rw, std::uint8_t value, char const *compat)
{
	std::string const j = millennium_audio_trace_voiceware_json(emulated_ns(), cpu_cycle, pc, event_type, port, rw,
		value, unsigned(m_bank & 0x0fU), unsigned(m_last_phrase), compat);
	millennium_audio_trace_emit_voiceware_row(j);
}

void millennium_voiceware_device::apply_rom_bank_from_latch()
{
	// 2 MiB region: 16 x 128 KiB banks (8 per U16 / U26). Lower nibble maps to uPD ROM bank.
	std::uint8_t const b = m_bank & 0x0fU;
	if (b == m_last_chip_bank)
		return;
	m_upd->set_rom_bank(b);
	m_last_chip_bank = b;
	// Board guidance (MAME upd7759 notes): reset after external ROM bank changes.
	m_upd->reset_w(0);
	m_upd->reset_w(1);
}

void millennium_voiceware_device::phrase_trace_attach_rom_fingerprint(coinline_voiceware_phrase_trace &tr) const
{
	if (!m_upd7759_core_env)
		return;
	tr.rom_sha256_u16 = m_voice_rom_sha256_u16.empty() ? nullptr : m_voice_rom_sha256_u16.c_str();
	tr.rom_sha256_u26 = m_voice_rom_sha256_u26.empty() ? nullptr : m_voice_rom_sha256_u26.c_str();
}

// Legacy software ADPCM → 48 kHz PCM buffer. Core path uses `upd7759_device` instead.
bool millennium_voiceware_device::decode_phrase(std::uint8_t phrase)
{
	memory_region *const vr = memregion(":voicew");
	if (!vr || !vr->base())
		return false;

	auto const *const rom = vr->base();
	u32 const rom_bytes = u32(vr->bytes());
	u32 const chip_base = ((m_bank & 0x08U) != 0U) ? 0x100000U : 0U;
	u32 const window_base = u32(m_bank & 0x07U) * 0x20000U;
	u32 stream_base = window_base;
	u32 constexpr window_bytes = 0x20000U;
	if (chip_base + window_base + 6U >= rom_bytes)
		return false;
	decode_mode const mode = decode_mode_from_env();

	auto read_bank = [&](u32 offset, u8 &value) -> bool {
		u32 const absolute = chip_base + stream_base + offset;
		if (absolute >= rom_bytes || offset >= window_bytes)
			return false;
		value = rom[absolute];
		return true;
	};

	u8 seg_last = 0;
	u8 m0 = 0;
	u8 m1 = 0;
	u8 m2 = 0;
	u8 m3 = 0;
	unsigned resolved_segment_index = unsigned(m_bank & 0x07U);
	unsigned msg_index = unsigned(phrase);
	unsigned msg_count = 0;
	if (mode == decode_mode::reference) {
		coinline_voiceware_phrase_lookup_result lr{};
		if (!coinline_voiceware_lookup_phrase_for_upd7759(rom, rom_bytes, m_bank & 0x0fU, phrase, lr))
			return false;
		stream_base = lr.segment_base_abs - chip_base;
		resolved_segment_index = lr.resolved_segment_index;
		msg_index = lr.phrase_msg_index;
		msg_count = lr.msg_count;
	} else {
		stream_base = window_base;
		resolved_segment_index = unsigned(m_bank & 0x07U);
	}
	if (!read_bank(0U, seg_last) || !read_bank(1U, m0) || !read_bank(2U, m1) || !read_bank(3U, m2) ||
		!read_bank(4U, m3))
		return false;
	bool const magic_ok = (m0 == 0x5aU && m1 == 0xa5U && m2 == 0x69U && m3 == 0x55U);
	if (mode != decode_mode::reference)
		msg_count = unsigned(seg_last) + 1U;
	u32 const dir_offset = 5U + 2U * msg_index;
	u8 hi = 0;
	u8 lo = 0;
	if (!read_bank(dir_offset, hi) || !read_bank(dir_offset + 1U, lo))
		return false;
	u32 const msg_word_be = u32((hi << 8) | lo);
	u32 const msg_offset = msg_word_be * 2U;
	u8 message_mode = 0xffU;
	if (!read_bank(msg_offset, message_mode))
		return false;
	u32 index = msg_offset + 1U;
	u8 cmd_preview[16] = {};
	for (unsigned i = 0; i < 16U; ++i) {
		u8 b = 0;
		if (read_bank(index + i, b))
			cmd_preview[i] = b;
	}
	u32 const segment_base = chip_base + stream_base;
	u32 const abs_msg_offset = segment_base + msg_offset;
	char const *const rom_label = ((m_bank & 0x08U) != 0U) ? "U26" : "U16";
	bool const verbose_decode = osd_getenv("COINLINE_TRACE_VOICEWARE_VERBOSE") &&
		*osd_getenv("COINLINE_TRACE_VOICEWARE_VERBOSE");
	append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
		std::string("{\"event\":\"phrase_start\",\"cycle\":") +
			std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
			hex_byte(phrase) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) + ",\"rom\":\"" +
			rom_label + "\"}");
	append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
		std::string("{\"event\":\"address_forensics\",\"cycle\":") +
			std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
			hex_byte(phrase) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) + ",\"rom\":\"" +
			rom_label + "\",\"segment_index\":" + std::to_string(resolved_segment_index) +
			",\"segment_base\":\"" + hex_offset(segment_base) + "\",\"segment_last\":\"" + hex_byte(seg_last) +
			"\",\"segment_magic_valid\":" + (magic_ok ? "true" : "false") + ",\"message_count\":" +
			std::to_string(msg_count) + ",\"phrase_index\":" + std::to_string(msg_index) +
			",\"offset_table_bytes\":{\"hi\":\"" + hex_byte(hi) + "\",\"lo\":\"" + hex_byte(lo) + "\"}," +
			"\"offset_table_endian\":\"big\",\"message_offset\":\"" + hex_offset(msg_offset) +
			"\",\"message_abs_offset\":\"" + hex_offset(abs_msg_offset) + "\",\"message_mode\":\"" +
			hex_byte(message_mode) + "\",\"first_commands\":[\"" + hex_byte(cmd_preview[0]) + "\",\"" +
			hex_byte(cmd_preview[1]) + "\",\"" + hex_byte(cmd_preview[2]) + "\",\"" + hex_byte(cmd_preview[3]) +
			"\",\"" + hex_byte(cmd_preview[4]) + "\",\"" + hex_byte(cmd_preview[5]) + "\",\"" +
			hex_byte(cmd_preview[6]) + "\",\"" + hex_byte(cmd_preview[7]) + "\",\"" + hex_byte(cmd_preview[8]) +
			"\",\"" + hex_byte(cmd_preview[9]) + "\",\"" + hex_byte(cmd_preview[10]) + "\",\"" +
			hex_byte(cmd_preview[11]) + "\",\"" + hex_byte(cmd_preview[12]) + "\",\"" +
			hex_byte(cmd_preview[13]) + "\",\"" + hex_byte(cmd_preview[14]) + "\",\"" +
			hex_byte(cmd_preview[15]) + "\"]}");
	append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
		std::string("{\"event\":\"directory_lookup\",\"cycle\":") +
			std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
			hex_byte(phrase) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) + ",\"rom\":\"" + rom_label +
			"\",\"dir_offset\":\"" + hex_offset(dir_offset) + "\",\"message_offset\":\"" + hex_offset(msg_offset) + "\"}");
	append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
		std::string("{\"event\":\"sample_offset\",\"cycle\":") +
			std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
			hex_byte(phrase) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) +
			",\"offset\":\"" + hex_offset(index) + "\"}");
	if (!magic_ok || msg_index >= msg_count || message_mode != 0x00U) {
		append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
			std::string("{\"event\":\"decode_warning\",\"cycle\":") +
				std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"offset\":\"" +
				hex_offset(msg_offset) + "\",\"control\":\"" + hex_byte(message_mode) +
				"\",\"decoder_mode\":\"container\",\"warning\":\"invalid_segment_or_message_mode\"}");
		return false;
	}

	auto const append_nibble = [](std::vector<std::int16_t> &pcm, u8 nibble, int &state, int &sample) {
		sample += k_upd7759_step[std::size_t(state)][std::size_t(nibble & 0x0fU)];
		state = std::clamp(state + k_upd7759_state[std::size_t(nibble & 0x0fU)], 0, 15);
		pcm.push_back(pcm_from_predictor(sample));
	};

	m_phrase_pcm.clear();
	m_phrase_pcm.reserve(64000);
	int state = 0;
	int sample = 0;
	char const *const mode_name = (mode == decode_mode::reference) ? "reference" : "spec";
	auto const trace_block = [&](u32 offset, u8 op, char const *type, unsigned sample_rate, unsigned count,
								 int state_before, int state_after, char const *note) {
		u8 const op_class = op & 0xc0U;
		append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
			std::string("{\"event\":\"block_start\",\"cycle\":") +
				std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
				hex_byte(phrase) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) + ",\"rom\":\"" +
				rom_label + "\",\"offset\":\"" + hex_offset(offset) + "\",\"raw_control\":\"" + hex_byte(op) +
				"\",\"op_class\":\"" + hex_byte(op_class) +
				"\",\"block_type\":\"" + type + "\",\"decoder_mode\":\"" + mode_name + "\",\"segment_index\":" +
				std::to_string(resolved_segment_index) + ",\"message_index\":" + std::to_string(msg_index) +
				",\"message_mode\":\"" + hex_byte(message_mode) + "\",\"sample_rate\":" +
				std::to_string(sample_rate) +
				",\"nibble_order\":\"msn_lsn\",\"sample_count\":" + std::to_string(count) +
				",\"state_before\":" + std::to_string(state_before) + ",\"state_after\":" +
				std::to_string(state_after) + ",\"note\":\"" + note + "\"}");
		append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
			std::string("{\"event\":\"native_rate_selected\",\"offset\":\"") + hex_offset(offset) + "\",\"op\":\"" +
				hex_byte(op) + "\",\"native_rate\":" + std::to_string(sample_rate) + "}");
		append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
			std::string("{\"event\":\"host_resample_ratio\",\"offset\":\"") + hex_offset(offset) + "\",\"op\":\"" +
				hex_byte(op) + "\",\"native_rate\":" + std::to_string(sample_rate) +
				",\"host_rate\":48000,\"ratio\":" + std::to_string(48000.0 / double(std::max(1U, sample_rate))) + "}");
	};
	auto const decode_nibble_block = [&](u32 data_offset, unsigned nibbles, std::vector<std::int16_t> &block_pcm,
										 int &state_ref, int &sample_ref, u32 block_offset, u8 op,
										 bool update_predictor) -> bool {
		unsigned pair_trace = 0U;
		while (nibbles > 0U) {
			u8 b = 0;
			if (!read_bank(data_offset++, b))
				return false;
			int const before_hi = state_ref;
			if (update_predictor) {
				append_nibble(block_pcm, u8(b >> 4), state_ref, sample_ref);
			} else {
				block_pcm.push_back(pcm_from_predictor(0));
			}
			if (verbose_decode || pair_trace < 4U) {
				append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
					std::string("{\"event\":\"adpcm_nibble_pair\",\"offset\":\"") + hex_offset(block_offset) +
						"\",\"control\":\"" + hex_byte(op) + "\",\"byte\":\"" + hex_byte(b) +
						"\",\"nibble_order\":\"msn_lsn\",\"state_before\":" + std::to_string(before_hi) +
						",\"state_after_hi\":" + std::to_string(state_ref) + "}");
			}
			--nibbles;
			if (nibbles > 0U) {
				int const before_lo = state_ref;
				if (update_predictor) {
					append_nibble(block_pcm, u8(b & 0x0fU), state_ref, sample_ref);
				} else {
					block_pcm.push_back(pcm_from_predictor(0));
				}
				if (verbose_decode) {
					append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
						std::string("{\"event\":\"adpcm_nibble\",\"nibble\":\"") + hex_byte(u8(b & 0x0fU)) +
							"\",\"state_before\":" + std::to_string(before_lo) + ",\"state_after\":" +
							std::to_string(state_ref) + "}");
				}
				--nibbles;
			}
			++pair_trace;
		}
		return true;
	};
	auto const decode_block_payload = [&](u32 data_offset, unsigned nibbles, std::vector<std::int16_t> &block_pcm,
										  u32 block_offset, u8 op) -> bool {
		unsigned remaining = nibbles;
		unsigned pair_trace = 0U;
		while (remaining > 0U) {
			u8 b = 0;
			if (!read_bank(data_offset++, b))
				return false;
			int const before_hi = state;
			append_nibble(block_pcm, u8(b >> 4), state, sample);
			if (verbose_decode || pair_trace < 4U) {
				append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
					std::string("{\"event\":\"adpcm_nibble_pair\",\"offset\":\"") + hex_offset(block_offset) +
						"\",\"control\":\"" + hex_byte(op) + "\",\"byte\":\"" + hex_byte(b) +
						"\",\"nibble_order\":\"msn_lsn\",\"state_before\":" + std::to_string(before_hi) +
						",\"state_after_hi\":" + std::to_string(state) + "}");
			}
			--remaining;
			if (remaining > 0U) {
				append_nibble(block_pcm, u8(b & 0x0fU), state, sample);
				--remaining;
			}
			++pair_trace;
		}
		return true;
	};
	unsigned repeat_count = 0;
	u32 repeat_offset = 0;
	for (unsigned guard = 0; guard < 0x20000U; ++guard) {
		if (mode == decode_mode::spec && repeat_count != 0U) {
			--repeat_count;
			index = repeat_offset;
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"repeat_loop_block\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(index) + "\",\"remaining\":" + std::to_string(repeat_count) +
					"}");
		}
		u8 op = 0;
		u32 const op_offset = index;
		if (!read_bank(index++, op))
			return false;
		if (op == 0x00U) {
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"end_marker\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"offset\":\"" +
					hex_offset(op_offset) + "\"}");
			break;
		}
		if (guard == 0) {
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"header_parsed\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) + "\"}");
		}
		int const state_before = state;
		int const sample_before = sample;
		u8 const op_class = op & 0xc0U;
		if (mode == decode_mode::reference && op >= 0x01U && op <= 0x3fU) {
			unsigned const native_samples = unsigned(op) * 8U;
			std::size_t const output_samples = std::max<std::size_t>(1U,
				std::size_t(std::llround(double(native_samples) * 48000.0 / 8000.0)));
			m_phrase_pcm.insert(m_phrase_pcm.end(), output_samples, 0);
			trace_block(op_offset, op, "silence", 8000U, unsigned(output_samples), state_before, state,
				"reference silence block");
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"silence_block\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"decoder_mode\":\"reference\",\"samples_emitted\":" + std::to_string(output_samples) + "}");
		} else if ((mode == decode_mode::reference && op >= 0x40U && op <= 0x7fU) || op_class == 0x40U) {
			u32 const block_rate = (mode == decode_mode::reference) ? 8000U : rate_from_header(op);
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"rate_marker\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"chosen_rate\":" + std::to_string(block_rate) + "}");
			std::vector<std::int16_t> block_pcm;
			block_pcm.reserve(256);
			if (mode == decode_mode::spec) {
				append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
					std::string("{\"event\":\"predictor_reset\",\"offset\":\"") + hex_offset(op_offset) +
						"\",\"reason\":\"fixed_adpcm_block\"}");
				state = 0;
				sample = 0;
			}
			if (!decode_block_payload(index, 256U, block_pcm, op_offset, op))
				return false;
			index += 128U;
			append_linear_resampled(m_phrase_pcm, block_pcm, block_rate);
			trace_block(op_offset, op, "fixed_256", block_rate, 256U, state_before, state,
				(mode == decode_mode::spec) ? "spec fixed ADPCM block; predictor reset at boundary"
											: "reference short ADPCM block");
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"adpcm_state_summary\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"predictor_before\":" + std::to_string(sample_before) +
					",\"predictor_after\":" + std::to_string(sample) +
					",\"index_before\":" + std::to_string(state_before) +
					",\"index_after\":" + std::to_string(state) +
					",\"samples_emitted\":" + std::to_string(block_pcm.size()) + "}");
		} else if (mode == decode_mode::spec && op_class == 0x00U) {
			state = 0;
			sample = 0;
			unsigned const native_samples = 256U * (unsigned(op & 0x3fU) + 1U);
			std::size_t const output_samples = std::max<std::size_t>(1U,
				std::size_t(std::llround(double(native_samples) * 48000.0 / 160000.0)));
			m_phrase_pcm.insert(m_phrase_pcm.end(), output_samples, 0);
			trace_block(op_offset, op, "silence", 160000U, unsigned(output_samples), state_before, state,
				"silence duration emitted at host stream rate");
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"silence_block\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"samples_emitted\":" + std::to_string(output_samples) + "}");
		} else if ((mode == decode_mode::reference && op >= 0x80U && op <= 0xbfU) || op_class == 0x80U) {
			u8 operand = 0;
			if (!read_bank(index++, operand))
				return false;
			unsigned const nibbles = unsigned(operand) + 1U;
			unsigned const byte_count = (nibbles + 1U) >> 1;
			u32 const block_rate = (mode == decode_mode::reference) ? 8000U : rate_from_header(op);
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"rate_marker\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"chosen_rate\":" + std::to_string(block_rate) + "}");
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"continuation_marker\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"length_semantics\":\"nibble_count_minus_one\",\"byte_count\":" + std::to_string(byte_count) +
					",\"nibbles\":" + std::to_string(nibbles) + "}");
			std::vector<std::int16_t> block_pcm;
			block_pcm.reserve(nibbles);
			if (mode == decode_mode::spec) {
				append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
					std::string("{\"event\":\"predictor_reset\",\"offset\":\"") + hex_offset(op_offset) +
						"\",\"reason\":\"variable_adpcm_block\"}");
				state = 0;
				sample = 0;
			}
			if (!decode_nibble_block(index, nibbles, block_pcm, state, sample, op_offset, op, true))
				return false;
			index += byte_count;
			append_linear_resampled(m_phrase_pcm, block_pcm, block_rate);
			trace_block(op_offset, op, "variable", block_rate, nibbles, state_before, state,
				(mode == decode_mode::spec)
					? "spec variable ADPCM block; nibble_count_minus_one payload; predictor reset at boundary"
					: "reference long ADPCM block; nibble_count_minus_one payload");
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"adpcm_state_summary\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"predictor_before\":" + std::to_string(sample_before) +
					",\"predictor_after\":" + std::to_string(sample) +
					",\"index_before\":" + std::to_string(state_before) +
					",\"index_after\":" + std::to_string(state) +
					",\"samples_emitted\":" + std::to_string(block_pcm.size()) + "}");
		} else if (mode == decode_mode::reference && op >= 0xc0U) {
			u8 operand = 0;
			if (!read_bank(index++, operand))
				return false;
			unsigned const nibbles = unsigned(operand) + 1U;
			unsigned const byte_count = (nibbles + 1U) >> 1;
			unsigned const repeats = unsigned((op >> 3U) & 0x07U);
			std::vector<std::int16_t> block_pcm;
			block_pcm.reserve(nibbles);
			if (!decode_nibble_block(index, nibbles, block_pcm, state, sample, op_offset, op, true))
				return false;
			index += byte_count;
			append_linear_resampled(m_phrase_pcm, block_pcm, 8000U);
			for (unsigned r = 0; r < repeats; ++r)
				append_linear_resampled(m_phrase_pcm, block_pcm, 8000U);
			trace_block(op_offset, op, "repeat_inline", 8000U, nibbles * (repeats + 1U), state_before, state,
				"reference repeat block with inline payload");
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"repeat_loop_block\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"decoder_mode\":\"reference\",\"repeat_count\":" + std::to_string(repeats) + "}");
		} else if (mode == decode_mode::spec && op_class == 0xc0U) {
			repeat_count = unsigned(op & 0x07U) + 1U;
			repeat_offset = index;
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"repeat_loop_block\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"repeat_count\":" + std::to_string(repeat_count) + "}");
		} else {
			append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
				std::string("{\"event\":\"decode_warning\",\"cycle\":") +
					std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
					",\"offset\":\"" + hex_offset(op_offset) + "\",\"control\":\"" + hex_byte(op) +
					"\",\"decoder_mode\":\"" + mode_name + "\",\"warning\":\"unknown_opcode_class\"}");
			if (mode == decode_mode::reference)
				continue;
			return false;
		}
	}
	m_phrase_pos = 0;
	append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
		std::string("{\"event\":\"phrase_end\",\"cycle\":") +
			std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
			hex_byte(phrase) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) +
			",\"pcm_samples\":" + std::to_string(m_phrase_pcm.size()) + "}");
	append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
		std::string("{\"event\":\"pcm_block\",\"cycle\":") +
			std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) +
			",\"sample_rate\":48000,\"samples\":" + std::to_string(m_phrase_pcm.size()) +
			",\"decoder_mode\":\"" + mode_name + "\",\"nibble_order\":\"msn_lsn\",\"predictor_mode\":\"" +
			std::string((mode == decode_mode::spec) ? "fresh_per_adpcm_block" : "preserve_across_blocks") + "\","
			"\"resample\":\"linear\",\"post_filter\":false}");
	return !m_phrase_pcm.empty();
}

void millennium_voiceware_device::write_hw_control(std::uint8_t data, std::uint16_t pc, std::uint64_t cpu_cycle)
{
	// Board-level voice reset control is inverted before reaching the uPD7759 /RESET pin.
	// Keep chip-level behavior active-low via reset_w(0)=asserted below.
	bool const rst = (data & VOICE_RESET_MASK) != 0U;
	bool const was_reset = m_reset_asserted;
	if (rst != m_reset_asserted)
		emit_row("voice_reset_edge", cpu_cycle, pc, 0x40, 'w', data, nullptr);
	m_reset_asserted = rst;

	// Active-low /RESET on uPD7759: reset_w(0) asserts reset.
	m_upd->reset_w(m_reset_asserted ? 0 : 1);

	if (m_reset_asserted) {
		m_bank_latched = false;
		m_last_chip_bank = 0xffU;
		m_decoder_playing = false;
		m_phrase_pcm.clear();
		m_phrase_pos = 0;
		m_audio_route->notify_voice_active(false);
	}
	else if (was_reset && m_bank_latched) {
		apply_rom_bank_from_latch();
	}
}

void millennium_voiceware_device::write_bank(std::uint8_t data, std::uint16_t pc, std::uint64_t cpu_cycle)
{
	(void)pc;
	m_bank = data & 0x0fU;
	m_bank_latched = true;
	if (!m_reset_asserted)
		apply_rom_bank_from_latch();
	emit_row("voiceware_bank_commit", cpu_cycle, pc, 0x42, 'w', data, nullptr);
}

std::uint8_t millennium_voiceware_device::read_phrase_port(std::uint16_t pc, std::uint64_t cpu_cycle)
{
	u16 const sp = u16(m_z180->state_int(Z180_SP) & 0xffffU);
	if (m_stream)
		m_stream->update();
	// Core path: MAME `busy_r()` is true when the uPD is idle (not shifting ADPCM).
	bool const idle = m_upd7759_core_env ? (m_reset_asserted || m_upd->busy_r()) : !m_decoder_playing;
	std::uint8_t const bus_value = m_reset_asserted ? m_phrase_port_levels.fault_read
							 : (idle ? m_phrase_port_levels.idle_read : m_phrase_port_levels.busy_read);
	if (m_had_playback && idle) {
		coinline_voiceware_phrase_trace tr{};
		tr.emulated_time_ns = emulated_ns();
		tr.cycle = cpu_cycle;
		tr.pc = pc;
		tr.sp = sp;
		tr.event_type = "voice_segment_complete";
		tr.port = 0x61;
		tr.rw = 'r';
		tr.value = bus_value;
		tr.raw_sample_index = m_last_phrase;
		tr.bank_latch = unsigned(m_bank & 0x0fU);
		tr.chip_bank = unsigned(m_last_chip_bank);
		tr.active_rom_id = (m_last_chip_bank < 8) ? "U16" : "U26";
		tr.active_blob = "unmapped_table";
		tr.mapping_confidence = "unknown";
		tr.upd7759_idle_before = false;
		tr.upd7759_idle_after = true;
		tr.playback_completed_edge = true;
		tr.execution_path = m_upd7759_core_env ? k_voiceware_exec_core : k_voiceware_exec_legacy;
		phrase_trace_attach_rom_fingerprint(tr);
		tr.notes = "mame_idle_transition on phrase status read; 0x61 data bus from phrase_port model (idle byte)";
		millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));
		m_audio_route->notify_voice_active(false);
	}
	m_had_playback = !idle;

	return bus_value;
}

void millennium_voiceware_device::write_phrase(std::uint8_t data, std::uint16_t pc, std::uint64_t cpu_cycle)
{
	u16 const sp = u16(m_z180->state_int(Z180_SP) & 0xffffU);
	if (m_stream)
		m_stream->update();
	bool const idle_before = m_upd7759_core_env ? (m_reset_asserted || m_upd->busy_r()) : !m_decoder_playing;
	if (m_reset_asserted) {
		m_last_phrase = data;
		emit_row("voice_fault_reset_active", cpu_cycle, pc, 0x61, 'w', data, nullptr);
		return;
	}
	if (!m_bank_latched) {
		m_bank = 0;
		m_bank_latched = true;
	}
	apply_rom_bank_from_latch();

	bool const playback_active = m_upd7759_core_env ? (!m_reset_asserted && !m_upd->busy_r()) : m_decoder_playing;
	if (m_retrigger_policy == coinline_voiceware_retrigger_policy::suppress_duplicate_strobe_while_playing
		&& playback_active && data == m_last_phrase && (m_bank & 0x0fU) == (m_last_chip_bank & 0x0fU)) {
		append_line_if_env("COINLINE_VOICEWARE_DECODE_TRACE",
			std::string("{\"event\":\"phrase_busy_ignored_retrigger\",\"cycle\":") +
				std::to_string(static_cast<unsigned long long>(m_z180->total_cycles())) + ",\"phrase\":\"" +
				hex_byte(data) + "\",\"bank\":" + std::to_string(unsigned(m_bank & 0x0fU)) + "}");
		coinline_voiceware_phrase_trace tr{};
		tr.emulated_time_ns = emulated_ns();
		tr.cycle = cpu_cycle;
		tr.pc = pc;
		tr.sp = sp;
		tr.event_type = "voice_segment_retrigger_ignored";
		tr.port = 0x61;
		tr.rw = 'w';
		tr.value = data;
		tr.raw_sample_index = data;
		tr.bank_latch = unsigned(m_bank & 0x0fU);
		tr.chip_bank = unsigned(m_last_chip_bank);
		tr.active_rom_id = (m_last_chip_bank < 8) ? "U16" : "U26";
		tr.active_blob = "unmapped_table";
		tr.mapping_confidence = "unknown";
		tr.upd7759_idle_before = false;
		tr.upd7759_idle_after = false;
		tr.playback_started = false;
		tr.execution_path = m_upd7759_core_env ? k_voiceware_exec_core : k_voiceware_exec_legacy;
		phrase_trace_attach_rom_fingerprint(tr);
		tr.notes = "duplicate active phrase strobe ignored (retrigger_policy=suppress); use "
			   "COINLINE_VOICEWARE_RETRIGGER_POLICY=allow to model lab restarts";
		millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));
		m_audio_route->notify_voice_active(true);
		return;
	}

	m_last_phrase = data;

	coinline_voiceware_phrase_trace tr{};
	tr.emulated_time_ns = emulated_ns();
	tr.cycle = cpu_cycle;
	tr.pc = pc;
	tr.sp = sp;
	tr.event_type = "voice_segment_start";
	tr.port = 0x61;
	tr.rw = 'w';
	tr.value = data;
	tr.raw_sample_index = data;
	tr.bank_latch = unsigned(m_bank & 0x0fU);
	tr.chip_bank = unsigned(m_last_chip_bank);
	tr.active_rom_id = (m_last_chip_bank < 8) ? "U16" : "U26";
	tr.active_blob = "unmapped_table";
	tr.mapping_confidence = "unknown";
	tr.upd7759_idle_before = idle_before;

	bool decoded = false;
	if (m_upd7759_core_env) {
		memory_region *const vr = memregion(":voicew");
		if (!vr || !vr->base() || vr->bytes() < 6U) {
			tr.execution_path = k_voiceware_exec_core;
			phrase_trace_attach_rom_fingerprint(tr);
			tr.notes = "core path: :voicew region missing";
			tr.upd7759_idle_after = m_upd->busy_r();
			tr.playback_started = false;
			m_decoder_playing = false;
			m_phrase_pcm.clear();
			m_phrase_pos = 0;
			millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));
			m_audio_route->notify_voice_active(false);
			return;
		}
		coinline_voiceware_phrase_lookup_result lr{};
		if (!coinline_voiceware_lookup_phrase_for_upd7759(vr->base(), u32(vr->bytes()), m_bank & 0x0fU, data, lr)) {
			if (coinline_voiceware_legacy_fallback_from_env()) {
				decoded = decode_phrase(data);
				m_decoder_playing = decoded;
				tr.execution_path = k_voiceware_exec_core_fallback;
				phrase_trace_attach_rom_fingerprint(tr);
				tr.notes = decoded ? "core lookup failed; legacy software decode (COINLINE_VOICEWARE_LEGACY_FALLBACK)"
						   : "core lookup failed; legacy decode also failed";
				tr.upd7759_idle_after = !m_decoder_playing;
				tr.playback_started = decoded;
				tr.audio_non_silent_known = decoded;
				tr.audio_non_silent = decoded && std::any_of(m_phrase_pcm.begin(), m_phrase_pcm.end(),
					[](std::int16_t sample) { return sample != 0; });
				millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));
				m_audio_route->notify_voice_active(decoded);
				return;
			}
			tr.execution_path = k_voiceware_exec_core;
			phrase_trace_attach_rom_fingerprint(tr);
			tr.notes = "core path: phrase lookup failed (no legacy fallback)";
			tr.upd7759_idle_after = m_upd->busy_r();
			tr.playback_started = false;
			m_decoder_playing = false;
			m_phrase_pcm.clear();
			m_phrase_pos = 0;
			millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));
			m_audio_route->notify_voice_active(false);
			return;
		}
		m_phrase_pcm.clear();
		m_phrase_pos = 0;
		m_decoder_playing = false;
		m_upd->port_w(lr.upd_sample_index);
		m_upd->start_w(1);
		m_upd->start_w(0);
		tr.execution_path = k_voiceware_exec_core;
		phrase_trace_attach_rom_fingerprint(tr);
		tr.notes = "upd7759 master start: port_w(sample index) + start pulse; PCM from device route";
		tr.upd7759_idle_after = m_upd->busy_r();
		tr.playback_started = true;
		tr.audio_non_silent_known = false;
		tr.audio_non_silent = false;
		millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));
		m_audio_route->notify_voice_active(true);
		return;
	}

	decoded = decode_phrase(data);
	m_decoder_playing = decoded;
	tr.execution_path = k_voiceware_exec_legacy;
	tr.notes = decoded
		? "phrase_byte decoded through rate-marker board ADPCM path"
		: "phrase_byte had no decodable board-format ADPCM stream in active bank";

	tr.upd7759_idle_after = !m_decoder_playing;
	tr.playback_started = decoded;
	tr.audio_non_silent_known = decoded;
	tr.audio_non_silent = decoded && std::any_of(m_phrase_pcm.begin(), m_phrase_pcm.end(),
		[](std::int16_t sample) { return sample != 0; });
	millennium_audio_trace_emit_voiceware_row(millennium_audio_trace_voiceware_phrase_detail_json(tr));

	m_audio_route->notify_voice_active(decoded);
}

bool millennium_voiceware_device::playing() const noexcept
{
	if (m_stream)
		m_stream->update();
	if (m_upd7759_core_env)
		return !m_reset_asserted && !m_upd->busy_r();
	return m_decoder_playing;
}
