// SPDX-License-Identifier: GPL-2.0-or-later
// Decoder-quality harness for local Voiceware ROM analysis.  Writes only
// derived WAVs/JSON metrics; never embeds ROM data in checked-in artifacts.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::array<std::array<int, 16>, 16> k_step = {{
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

constexpr std::array<int, 16> k_state = {{ -1, -1, 0, 0, 1, 2, 2, 3, -1, -1, 0, 0, 1, 2, 2, 3 }};

enum class parser_mode { archive_priority, standard_upd };
enum class nibble_order { msn_lsn, lsn_msn };
enum class resample_mode { native_marker, hold_48k, linear_48k, lowpass_48k };
enum class variable_length_mode { nibble_count_minus_one, byte_count_minus_one };

struct candidate_cfg {
	const char *name;
	parser_mode parser;
	bool whole_chip;
	nibble_order order;
	bool phase_shift;
	bool reset_each_block;
	bool use_rate_markers;
	resample_mode resample;
	bool silence_resets;
	variable_length_mode variable_length;
};

struct metrics {
	bool ok = false;
	bool non_silent = false;
	std::size_t samples = 0;
	int peak = 0;
	double rms = 0.0;
	double dc = 0.0;
	std::size_t clipped = 0;
	std::size_t zero_crossings = 0;
	double silence_pct = 100.0;
	double speech_likeness = 0.0;
};

struct report {
	candidate_cfg cfg;
	std::string wav;
	std::string rom_label;
	std::uint8_t phrase = 0;
	unsigned bank = 0;
	std::uint32_t directory_offset = 0;
	std::uint32_t start_offset = 0;
	std::string header_summary;
	std::map<std::string, unsigned> block_types;
	std::vector<unsigned> rates;
	std::vector<std::string> unknown_markers;
	std::string fatal;
	std::string notes;
	std::size_t blocks = 0;
	std::uint32_t output_rate = 0;
	metrics m;
};

std::string q(std::string const &s)
{
	std::string out = "\"";
	for (char c : s) {
		if (c == '\\' || c == '"')
			out.push_back('\\');
		out.push_back(c);
	}
	out.push_back('"');
	return out;
}

std::string hex_byte(unsigned v)
{
	std::ostringstream s;
	s << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (v & 0xffU);
	return s.str();
}

std::string hex_off(std::uint32_t v)
{
	std::ostringstream s;
	s << "0x" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << v;
	return s.str();
}

bool read_file(std::filesystem::path const &path, std::vector<std::uint8_t> &out)
{
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return false;
	f.seekg(0, std::ios::end);
	auto const size = f.tellg();
	if (size < 0)
		return false;
	out.resize(static_cast<std::size_t>(size));
	f.seekg(0);
	if (!out.empty())
		f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
	return static_cast<bool>(f);
}

void put_u16(std::ostream &o, std::uint16_t v)
{
	o.put(char(v & 0xffU));
	o.put(char((v >> 8) & 0xffU));
}

void put_u32(std::ostream &o, std::uint32_t v)
{
	put_u16(o, std::uint16_t(v & 0xffffU));
	put_u16(o, std::uint16_t(v >> 16));
}

bool write_wav(std::filesystem::path const &path, std::vector<std::int16_t> const &pcm, std::uint32_t rate)
{
	std::ofstream o(path, std::ios::binary);
	if (!o)
		return false;
	std::uint32_t const data_bytes = std::uint32_t(pcm.size() * sizeof(std::int16_t));
	o.write("RIFF", 4);
	put_u32(o, 36U + data_bytes);
	o.write("WAVEfmt ", 8);
	put_u32(o, 16);
	put_u16(o, 1);
	put_u16(o, 1);
	put_u32(o, rate);
	put_u32(o, rate * 2U);
	put_u16(o, 2);
	put_u16(o, 16);
	o.write("data", 4);
	put_u32(o, data_bytes);
	o.write(reinterpret_cast<char const *>(pcm.data()), static_cast<std::streamsize>(data_bytes));
	return static_cast<bool>(o);
}

std::int16_t pcm_from_predictor(int sample)
{
	return std::int16_t(std::clamp(sample * 128, -32768, 32767));
}

std::uint32_t rate_from_header(std::uint8_t op)
{
	// 160 kHz is the chip timing base, not the WAV/host output rate.
	unsigned const divider = unsigned(op & 0x3fU) + 1U;
	return std::max<std::uint32_t>(1U, 160000U / divider);
}

void update_adpcm(std::vector<std::int16_t> &pcm, std::uint8_t nibble, int &state, int &sample)
{
	sample += k_step[std::size_t(state)][std::size_t(nibble & 0x0fU)];
	state = std::clamp(state + k_state[std::size_t(nibble & 0x0fU)], 0, 15);
	pcm.push_back(pcm_from_predictor(sample));
}

void append_resampled(std::vector<std::int16_t> &dst, std::vector<std::int16_t> const &src, std::uint32_t src_rate,
	resample_mode mode)
{
	if (src.empty())
		return;
	if (mode == resample_mode::native_marker) {
		dst.insert(dst.end(), src.begin(), src.end());
		return;
	}
	constexpr std::uint32_t dst_rate = 48000;
	std::size_t const count = std::max<std::size_t>(1U,
		std::size_t(std::llround(double(src.size()) * double(dst_rate) / double(src_rate))));
	double lp = dst.empty() ? 0.0 : double(dst.back());
	for (std::size_t i = 0; i < count; ++i) {
		double const pos = double(i) * double(src_rate) / double(dst_rate);
		std::size_t const base = std::min<std::size_t>(std::size_t(pos), src.size() - 1U);
		double y = double(src[base]);
		if (mode != resample_mode::hold_48k) {
			std::size_t const next = std::min<std::size_t>(base + 1U, src.size() - 1U);
			double const frac = pos - double(base);
			y = double(src[base]) * (1.0 - frac) + double(src[next]) * frac;
			if (mode == resample_mode::lowpass_48k) {
				lp += 0.35 * (y - lp);
				y = lp;
			}
		}
		dst.push_back(std::int16_t(std::clamp<int>(int(std::lround(y)), -32768, 32767)));
	}
}

metrics analyze(std::vector<std::int16_t> const &pcm, std::uint32_t rate, std::string const &fatal)
{
	metrics m;
	m.samples = pcm.size();
	if (pcm.empty())
		return m;
	long double sum = 0.0;
	long double sum2 = 0.0;
	std::size_t quiet = 0;
	int prev = pcm[0];
	for (std::size_t i = 0; i < pcm.size(); ++i) {
		int const v = int(pcm[i]);
		m.peak = std::max(m.peak, std::abs(v));
		m.clipped += (v == -32768 || v == 32767) ? 1U : 0U;
		m.non_silent = m.non_silent || (v != 0);
		quiet += std::abs(v) < 256 ? 1U : 0U;
		sum += v;
		sum2 += static_cast<long double>(v) * static_cast<long double>(v);
		if (i && ((prev < 0 && v >= 0) || (prev >= 0 && v < 0)))
			++m.zero_crossings;
		prev = v;
	}
	m.ok = m.non_silent && fatal.empty();
	m.rms = std::sqrt(double(sum2 / pcm.size()));
	m.dc = double(sum / pcm.size());
	m.silence_pct = 100.0 * double(quiet) / double(pcm.size());
	double const duration = rate ? double(pcm.size()) / double(rate) : 0.0;
	double score = 0.0;
	if (m.ok) score += 25.0;
	if (duration > 0.08 && duration < 4.0) score += 15.0;
	if (m.peak > 512 && m.peak < 32768) score += 15.0;
	score += std::max(0.0, 15.0 - std::abs(m.dc) / 512.0);
	score += std::max(0.0, 15.0 - double(m.clipped) * 2.0);
	double const zcr = duration > 0.0 ? double(m.zero_crossings) / duration : 0.0;
	if (zcr > 20.0 && zcr < 2500.0) score += 10.0;
	if (m.silence_pct < 80.0) score += 5.0;
	m.speech_likeness = std::clamp(score, 0.0, 100.0);
	return m;
}

std::string rates_json(std::vector<unsigned> const &rates)
{
	std::vector<unsigned> unique = rates;
	std::sort(unique.begin(), unique.end());
	unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
	std::ostringstream s;
	s << "[";
	for (std::size_t i = 0; i < unique.size(); ++i)
		s << (i ? "," : "") << unique[i];
	s << "]";
	return s.str();
}

void trace_event(std::ofstream &trace, std::string const &event, report const &r, std::uint32_t offset,
	unsigned block, std::uint8_t control, std::string const &extra = {})
{
	if (!trace)
		return;
	trace << "{\"event\":\"" << event << "\",\"phrase\":\"" << hex_byte(r.phrase) << "\",\"bank\":" << r.bank
		  << ",\"rom\":\"" << r.rom_label << "\",\"offset\":\"" << hex_off(offset) << "\",\"block_index\":"
		  << block << ",\"control\":\"" << hex_byte(control) << "\"";
	if (!extra.empty())
		trace << "," << extra;
	trace << "}\n";
}

bool decode_candidate(std::vector<std::uint8_t> const &chip_rom, unsigned global_bank, std::uint8_t phrase,
	candidate_cfg cfg, std::filesystem::path const &outdir, std::ofstream &trace, report &r)
{
	r.cfg = cfg;
	r.phrase = phrase;
	r.bank = global_bank;
	r.rom_label = global_bank < 8 ? "U16" : "U26";
	r.wav = std::string("candidate-") + cfg.name + ".wav";
	std::vector<std::uint8_t> const *decode_rom = &chip_rom;
	std::vector<std::uint8_t> bank_rom;
	if (!cfg.whole_chip) {
		std::size_t const base = (global_bank & 7U) * 0x20000U;
		if (base + 0x20000U > chip_rom.size()) {
			r.fatal = "bank_oob";
			return false;
		}
		bank_rom.assign(chip_rom.begin() + base, chip_rom.begin() + base + 0x20000U);
		decode_rom = &bank_rom;
	}
	auto const &rom = *decode_rom;
	if (5U + 2U * phrase + 1U >= rom.size()) {
		r.fatal = "directory_oob";
		return false;
	}
	std::ostringstream hs;
	for (std::size_t i = 0; i < 8 && i < rom.size(); ++i)
		hs << (i ? " " : "") << hex_byte(rom[i]);
	r.header_summary = hs.str();
	r.directory_offset = 5U + 2U * phrase;
	r.start_offset = ((std::uint32_t(rom[r.directory_offset]) << 8) | rom[r.directory_offset + 1U]) * 2U + 1U;
	if (r.start_offset >= rom.size()) {
		r.fatal = "sample_offset_oob";
		return false;
	}
	trace_event(trace, "phrase_start", r, 0, 0, 0, std::string("\"candidate\":") + q(cfg.name));
	trace_event(trace, "bank_selected", r, 0, 0, 0, std::string("\"scope\":") + q(cfg.whole_chip ? "whole_chip" : "128k_bank"));
	trace_event(trace, "directory_lookup", r, r.directory_offset, 0, 0, std::string("\"decoded_offset\":\"") + hex_off(r.start_offset) + "\"");
	trace_event(trace, "sample_offset", r, r.start_offset, 0, rom[r.start_offset]);
	std::vector<std::int16_t> out;
	std::uint32_t native_header_rate = 8421;
	std::uint32_t index = r.start_offset;
	int state = 0;
	int sample = 0;
	bool first_valid = false;
	bool dropped_phase = !cfg.phase_shift;
	unsigned repeat_count = 0;
	std::uint32_t repeat_offset = 0;
	for (unsigned guard = 0; guard < 4096 && index < rom.size(); ++guard) {
		std::uint32_t const opoff = index;
		if (cfg.parser == parser_mode::standard_upd && repeat_count) {
			--repeat_count;
			index = repeat_offset;
			trace_event(trace, "repeat_loop_block", r, opoff, unsigned(r.blocks), 0, "\"warning\":\"repeat_rewind\"");
		}
		std::uint8_t const op = rom[index++];
		if (r.blocks == 0) {
			trace_event(trace, "header_parsed", r, opoff, 0, op);
			native_header_rate = cfg.use_rate_markers && (op & 0x40U) ? rate_from_header(op) : 8421U;
		}
		if (op == 0x00U && (cfg.parser == parser_mode::archive_priority || first_valid)) {
			trace_event(trace, "end_marker", r, opoff, unsigned(r.blocks), op);
			break;
		}
		first_valid = first_valid || op != 0x00U;
		int const state_before = state;
		int const sample_before = sample;
		auto emit_block = [&](char const *type, std::vector<std::uint8_t> const &bytes, std::uint32_t rate) {
			std::vector<std::int16_t> block_pcm;
			if (cfg.reset_each_block) {
				state = 0;
				sample = 0;
			}
			unsigned pair_trace = 0;
			for (std::uint8_t b : bytes) {
				std::array<std::uint8_t, 2> ns = cfg.order == nibble_order::msn_lsn
					? std::array<std::uint8_t, 2>{{ std::uint8_t(b >> 4), std::uint8_t(b & 0x0fU) }}
					: std::array<std::uint8_t, 2>{{ std::uint8_t(b & 0x0fU), std::uint8_t(b >> 4) }};
				for (std::uint8_t n : ns) {
					if (!dropped_phase) {
						dropped_phase = true;
						continue;
					}
					update_adpcm(block_pcm, n, state, sample);
				}
				if (pair_trace++ < 4U)
					trace_event(trace, "adpcm_nibble_pair", r, opoff, unsigned(r.blocks), op,
						std::string("\"byte\":\"") + hex_byte(b) + "\",\"nibble_order\":\"" +
						(cfg.order == nibble_order::msn_lsn ? "msn_lsn" : "lsn_msn") + "\"");
			}
			append_resampled(out, block_pcm, rate, cfg.resample);
			r.block_types[type]++;
			r.rates.push_back(rate);
			trace_event(trace, "adpcm_state_summary", r, opoff, unsigned(r.blocks), op,
				"\"predictor_before\":" + std::to_string(sample_before) + ",\"predictor_after\":" +
				std::to_string(sample) + ",\"index_before\":" + std::to_string(state_before) +
				",\"index_after\":" + std::to_string(state) + ",\"samples_emitted\":" +
				std::to_string(block_pcm.size()));
		};
		if (cfg.parser == parser_mode::archive_priority ? ((op & 0x40U) != 0U) : ((op & 0xc0U) == 0x40U)) {
			std::vector<std::uint8_t> bytes;
			for (unsigned i = 0; i < 128U && index < rom.size(); ++i)
				bytes.push_back(rom[index++]);
			std::uint32_t const rate = cfg.use_rate_markers ? rate_from_header(op) : 8421U;
			trace_event(trace, "rate_marker", r, opoff, unsigned(r.blocks), op, "\"chosen_rate\":" + std::to_string(rate));
			trace_event(trace, "block_start", r, opoff, unsigned(r.blocks), op, "\"block_type\":\"fixed_256\"");
			emit_block("fixed_256", bytes, rate);
		} else if ((op & 0xc0U) == 0x00U) {
			if (cfg.silence_resets) {
				state = 0;
				sample = 0;
			}
			unsigned const native_samples = 256U * (unsigned(op & 0x3fU) + 1U);
			std::vector<std::int16_t> zeros(native_samples, 0);
			append_resampled(out, zeros, 160000U, cfg.resample);
			r.block_types["silence"]++;
			trace_event(trace, "silence_block", r, opoff, unsigned(r.blocks), op,
				std::string("\"warning\":\"") + (cfg.silence_resets ? "predictor_reset" : "spacing") +
				"\",\"samples_emitted\":" + std::to_string(native_samples));
		} else if ((op & 0xc0U) == 0x80U) {
			if (index >= rom.size()) {
				r.fatal = "variable_count_oob";
				break;
			}
			unsigned const operand = unsigned(rom[index++]);
			unsigned const nibbles = cfg.variable_length == variable_length_mode::byte_count_minus_one
				? 2U * (operand + 1U)
				: operand + 1U;
			unsigned const decoded_nibbles = nibbles & ~1U;
			unsigned const byte_count = decoded_nibbles >> 1;
			std::vector<std::uint8_t> bytes;
			for (unsigned i = 0; i < byte_count && index < rom.size(); ++i)
				bytes.push_back(rom[index++]);
			std::uint32_t const rate = cfg.use_rate_markers ? rate_from_header(op) : 8421U;
			trace_event(trace, "rate_marker", r, opoff, unsigned(r.blocks), op, "\"chosen_rate\":" + std::to_string(rate));
			trace_event(trace, "continuation_marker", r, opoff, unsigned(r.blocks), op,
				std::string("\"length_semantics\":\"") +
				(cfg.variable_length == variable_length_mode::byte_count_minus_one ? "byte_count_minus_one" : "nibble_count_minus_one") +
				"\",\"byte_count\":" + std::to_string(byte_count) + ",\"nibbles\":" + std::to_string(nibbles));
			emit_block("variable", bytes, rate);
		} else if (cfg.parser == parser_mode::standard_upd) {
			repeat_count = (op & 0x07U) + 1U;
			repeat_offset = index;
			r.block_types["repeat_loop"]++;
			trace_event(trace, "repeat_loop_block", r, opoff, unsigned(r.blocks), op, "\"mode\":\"standard_replay_next_header\"");
		} else {
			r.unknown_markers.push_back(hex_byte(op));
			trace_event(trace, "decode_warning", r, opoff, unsigned(r.blocks), op, "\"warning\":\"unknown_marker\"");
		}
		++r.blocks;
	}
	r.output_rate = cfg.resample == resample_mode::native_marker ? native_header_rate : 48000U;
	r.m = analyze(out, r.output_rate, r.fatal);
	if (cfg.resample == resample_mode::native_marker)
		r.notes = "native-rate diagnostic uses first ADPCM block rate as WAV header when markers vary";
	write_wav(outdir / r.wav, out, r.output_rate);
	trace_event(trace, "phrase_end", r, index, unsigned(r.blocks), 0,
		"\"output_samples\":" + std::to_string(out.size()) + ",\"output_rate\":" + std::to_string(r.output_rate));
	return r.m.ok;
}

void write_candidate_json(std::filesystem::path const &path, report const &r)
{
	double const duration = (r.output_rate ? double(r.m.samples) / double(r.output_rate) : 0.0);
	bool const parse_completed = r.fatal.empty();
	bool const duration_plausible = (duration >= 0.08 && duration <= 8.0);
	bool const expected_duration_match = parse_completed && duration_plausible;
	bool const fast_or_chopped = (duration < 0.15) || (r.m.zero_crossings > r.output_rate * 0.20);
	bool const noise_like = (r.m.clipped > 0) || (r.m.peak >= 32000) || (std::abs(r.m.dc) > 4000.0);
	std::ofstream o(path);
	o << "{\n"
	  << "  \"candidate\":" << q(r.cfg.name) << ",\n"
	  << "  \"phrase\":" << q(hex_byte(r.phrase)) << ",\n"
	  << "  \"bank\":" << r.bank << ",\n"
	  << "  \"ROM\":" << q(r.rom_label) << ",\n"
	  << "  \"directory_offset\":" << q(hex_off(r.directory_offset)) << ",\n"
	  << "  \"decoded_start_offset\":" << q(hex_off(r.start_offset)) << ",\n"
	  << "  \"header_bytes_summary\":" << q(r.header_summary) << ",\n"
	  << "  \"block_count\":" << r.blocks << ",\n"
	  << "  \"block_types_seen\":{";
	bool first = true;
	for (auto const &kv : r.block_types) {
		o << (first ? "" : ",") << q(kv.first) << ":" << kv.second;
		first = false;
	}
	o << "},\n"
	  << "  \"sample_rate_markers_seen\":" << rates_json(r.rates) << ",\n"
	  << "  \"nibble_order\":" << q(r.cfg.order == nibble_order::msn_lsn ? "msn_lsn" : "lsn_msn") << ",\n"
	  << "  \"variable_length_semantics\":"
	  << q(r.cfg.variable_length == variable_length_mode::byte_count_minus_one ? "byte_count_minus_one" : "nibble_count_minus_one") << ",\n"
	  << "  \"predictor_index_mode\":" << q(r.cfg.reset_each_block ? "fresh_predictor_each_block" : "preserve_across_blocks") << ",\n"
	  << "  \"output_sample_count\":" << r.m.samples << ",\n"
	  << "  \"output_sample_rate\":" << r.output_rate << ",\n"
	  << "  \"duration\":" << (r.output_rate ? double(r.m.samples) / double(r.output_rate) : 0.0) << ",\n"
	  << "  \"peak\":" << r.m.peak << ",\n"
	  << "  \"RMS\":" << r.m.rms << ",\n"
	  << "  \"DC_offset\":" << r.m.dc << ",\n"
	  << "  \"zero_crossing_rate\":" << (r.output_rate && r.m.samples ? double(r.m.zero_crossings) / (double(r.m.samples) / double(r.output_rate)) : 0.0) << ",\n"
	  << "  \"clipping_count\":" << r.m.clipped << ",\n"
	  << "  \"silence_percentage\":" << r.m.silence_pct << ",\n"
	  << "  \"unknown_markers\":[";
	for (std::size_t i = 0; i < r.unknown_markers.size(); ++i)
		o << (i ? "," : "") << q(r.unknown_markers[i]);
	o << "],\n"
	  << "  \"parse_completed\":" << (parse_completed ? "true" : "false") << ",\n"
	  << "  \"unknown_opcode_count\":" << r.unknown_markers.size() << ",\n"
	  << "  \"expected_duration_match\":" << (expected_duration_match ? "true" : "false") << ",\n"
	  << "  \"fast_or_chopped\":" << (fast_or_chopped ? "true" : "false") << ",\n"
	  << "  \"corrupted_or_noise_like\":" << (noise_like ? "true" : "false") << ",\n"
	  << "  \"fatal_parse_errors\":" << q(r.fatal) << ",\n"
	  << "  \"speech_likeness_score\":" << r.m.speech_likeness << ",\n"
	  << "  \"wav\":" << q(r.wav) << ",\n"
	  << "  \"notes\":" << q(r.notes) << "\n"
	  << "}\n";
}

void write_summary(std::filesystem::path const &path, std::vector<report> const &reports)
{
	auto best = std::max_element(reports.begin(), reports.end(), [](report const &a, report const &b) {
		return a.m.speech_likeness < b.m.speech_likeness;
	});
	std::ofstream o(path);
	o << "{\n  \"schema_version\":\"coinline.voiceware_ab/v1\",\n"
	  << "  \"selected_candidate\":" << (best == reports.end() ? "\"\"" : q(best->cfg.name)) << ",\n"
	  << "  \"selection_reason\":\"highest objective speech_likeness_score; listen to WAV before production promotion\",\n"
	  << "  \"candidates\":[\n";
	for (std::size_t i = 0; i < reports.size(); ++i) {
		auto const &r = reports[i];
		o << "    {\"name\":" << q(r.cfg.name) << ",\"wav\":" << q(r.wav)
		  << ",\"score\":" << r.m.speech_likeness << ",\"ok\":" << (r.m.ok ? "true" : "false")
		  << ",\"rate\":" << r.output_rate << ",\"duration\":"
		  << (r.output_rate ? double(r.m.samples) / double(r.output_rate) : 0.0)
		  << ",\"peak\":" << r.m.peak << ",\"rms\":" << r.m.rms << ",\"dc\":" << r.m.dc
		  << ",\"clipped\":" << r.m.clipped << ",\"fatal\":" << q(r.fatal) << "}";
		o << (i + 1U == reports.size() ? "\n" : ",\n");
	}
	o << "  ]\n}\n";
}

} // namespace

int main(int argc, char **argv)
{
	if (argc < 6) {
		std::cerr << "usage: voiceware_decoder_ab <u16.bin> <u26.bin> <outdir> <phrase_hex> <bank_dec>\n";
		return 2;
	}
	std::filesystem::path const u16 = argv[1];
	std::filesystem::path const u26 = argv[2];
	std::filesystem::path const outdir = argv[3];
	unsigned const phrase = std::stoul(argv[4], nullptr, 0) & 0xffU;
	unsigned const bank = std::stoul(argv[5], nullptr, 0) & 0x0fU;
	std::filesystem::create_directories(outdir);
	std::vector<std::uint8_t> rom;
	char const *rom_label = bank < 8 ? "U16" : "U26";
	if (!read_file(bank < 8 ? u16 : u26, rom)) {
		std::cerr << "failed to read selected ROM\n";
		return 1;
	}
	std::ofstream trace(outdir / "voiceware-decode-trace.jsonl");
	std::vector<candidate_cfg> cfgs = {
		{ "reference_archive_fixed8421_native", parser_mode::archive_priority, true, nibble_order::msn_lsn, false, false, false, resample_mode::native_marker, false, variable_length_mode::nibble_count_minus_one },
		{ "current_archive_rate_hold48", parser_mode::archive_priority, true, nibble_order::msn_lsn, false, false, true, resample_mode::hold_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "archive_rate_linear48", parser_mode::archive_priority, true, nibble_order::msn_lsn, false, false, true, resample_mode::linear_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "archive_rate_lowpass48", parser_mode::archive_priority, true, nibble_order::msn_lsn, false, false, true, resample_mode::lowpass_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "archive_lsn_rate_linear48", parser_mode::archive_priority, true, nibble_order::lsn_msn, false, false, true, resample_mode::linear_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "archive_phase1_rate_linear48", parser_mode::archive_priority, true, nibble_order::msn_lsn, true, false, true, resample_mode::linear_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "archive_reset_blocks_rate_linear48", parser_mode::archive_priority, true, nibble_order::msn_lsn, false, true, true, resample_mode::linear_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "standard_upd_rate_hold48", parser_mode::standard_upd, true, nibble_order::msn_lsn, false, false, true, resample_mode::hold_48k, true, variable_length_mode::nibble_count_minus_one },
		{ "bank128_archive_rate_linear48", parser_mode::archive_priority, false, nibble_order::msn_lsn, false, false, true, resample_mode::linear_48k, false, variable_length_mode::nibble_count_minus_one },
		{ "bank128_spec_nibble_rate_linear48", parser_mode::standard_upd, false, nibble_order::msn_lsn, false, true, true, resample_mode::linear_48k, true, variable_length_mode::nibble_count_minus_one },
		{ "bank128_spec_byte_rate_linear48", parser_mode::standard_upd, false, nibble_order::msn_lsn, false, true, true, resample_mode::linear_48k, true, variable_length_mode::byte_count_minus_one },
		{ "bank128_spec_nibble_native", parser_mode::standard_upd, false, nibble_order::msn_lsn, false, true, true, resample_mode::native_marker, true, variable_length_mode::nibble_count_minus_one },
	};
	std::vector<report> reports;
	for (auto const &cfg : cfgs) {
		report r{};
		decode_candidate(rom, bank, std::uint8_t(phrase), cfg, outdir, trace, r);
		write_candidate_json(outdir / (std::string("candidate-") + cfg.name + ".json"), r);
		reports.push_back(r);
	}
	write_summary(outdir / "voiceware-ab-summary.json", reports);
	std::cout << "OK: voiceware decoder AB " << outdir.string() << " rom=" << rom_label << "\n";
	return 0;
}
