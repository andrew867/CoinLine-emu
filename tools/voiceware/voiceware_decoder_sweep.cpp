// SPDX-License-Identifier: GPL-2.0-or-later
// Local Voiceware decoder comparison utility.  It writes only derived audio
// metrics and WAVs; it never embeds ROM bytes in checked-in artifacts.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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

struct decode_report {
	std::string name;
	std::string wav;
	bool ok = false;
	bool non_silent = false;
	std::uint32_t sample_rate = 0;
	std::uint32_t start_offset = 0;
	std::uint8_t first_header = 0;
	std::size_t samples = 0;
	int peak = 0;
	double rms = 0.0;
	double dc = 0.0;
	std::size_t clipped = 0;
	std::size_t zero_crossings = 0;
	std::size_t longest_zero_run = 0;
	std::size_t longest_internal_zero_run = 0;
	std::size_t long_zero_runs = 0;
	std::size_t long_internal_zero_runs = 0;
	std::size_t longest_quiet_run = 0;
	std::size_t long_quiet_runs = 0;
	std::size_t first_nonzero = 0;
	std::size_t last_nonzero = 0;
	std::size_t blocks = 0;
	std::string first_block_type;
	std::string warnings;
};

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

void append_sample(std::vector<std::int16_t> &pcm, std::uint8_t nibble, unsigned repeats, int &state, int &sample)
{
	sample += k_step[std::size_t(state)][std::size_t(nibble & 0x0fU)];
	state = std::clamp(state + k_state[std::size_t(nibble & 0x0fU)], 0, 15);
	std::int16_t const s = std::int16_t(sample << 7);
	for (unsigned i = 0; i < repeats; ++i)
		pcm.push_back(s);
}

void append_byte_nibbles(std::vector<std::int16_t> &pcm, std::uint8_t b, unsigned repeats, int &state, int &sample,
	bool lsn_first, bool flip_sign_bit)
{
	std::uint8_t hi = std::uint8_t(b >> 4);
	std::uint8_t lo = std::uint8_t(b & 0x0fU);
	if (flip_sign_bit) {
		hi ^= 0x08U;
		lo ^= 0x08U;
	}
	if (lsn_first) {
		append_sample(pcm, lo, repeats, state, sample);
		append_sample(pcm, hi, repeats, state, sample);
	} else {
		append_sample(pcm, hi, repeats, state, sample);
		append_sample(pcm, lo, repeats, state, sample);
	}
}

std::string hex_byte(std::uint8_t v)
{
	std::ostringstream s;
	s << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << unsigned(v);
	return s.str();
}

std::string hex_off(std::uint32_t v)
{
	std::ostringstream s;
	s << "0x" << std::uppercase << std::hex << std::setw(6) << std::setfill('0') << v;
	return s.str();
}

void trace_block(std::ofstream &trace, char const *decoder, unsigned phrase, unsigned bank, char const *rom_label,
	std::uint32_t offset, std::uint8_t op, char const *type, unsigned rate, unsigned count, int state_before,
	int state_after, char const *note)
{
	if (!trace)
		return;
	trace << "{\"event\":\"block_start\",\"decoder\":\"" << decoder << "\",\"phrase\":" << phrase
		  << ",\"bank\":" << bank << ",\"rom\":\"" << rom_label << "\",\"offset\":\"" << hex_off(offset)
		  << "\",\"raw_control\":\"" << hex_byte(op) << "\",\"block_type\":\"" << type
		  << "\",\"sample_rate\":" << rate << ",\"nibble_order\":\"msn_lsn\",\"sample_count\":" << count
		  << ",\"state_before\":" << state_before << ",\"state_after\":" << state_after
		  << ",\"note\":\"" << note << "\"}\n";
}

void compute_metrics(decode_report &r, std::vector<std::int16_t> const &pcm)
{
	r.samples = pcm.size();
	long double sum = 0.0;
	long double sum2 = 0.0;
	int prev = 0;
	bool have_prev = false;
	bool saw_nonzero = false;
	std::size_t zero_run = 0;
	std::size_t internal_zero_run = 0;
	std::size_t pos = 0;
	for (std::int16_t v : pcm) {
		int const iv = int(v);
		r.peak = std::max(r.peak, std::abs(iv));
		r.clipped += (iv == -32768 || iv == 32767) ? 1U : 0U;
		r.non_silent = r.non_silent || (iv != 0);
		sum += iv;
		sum2 += static_cast<long double>(iv) * static_cast<long double>(iv);
		if (have_prev && ((prev < 0 && iv >= 0) || (prev >= 0 && iv < 0)))
			++r.zero_crossings;
		if (iv == 0) {
			++zero_run;
			if (saw_nonzero)
				++internal_zero_run;
		} else if (zero_run > 0) {
			r.longest_zero_run = std::max(r.longest_zero_run, zero_run);
			r.long_zero_runs += zero_run >= 128U ? 1U : 0U;
			if (internal_zero_run > 0) {
				r.longest_internal_zero_run = std::max(r.longest_internal_zero_run, internal_zero_run);
				r.long_internal_zero_runs += internal_zero_run >= 128U ? 1U : 0U;
				internal_zero_run = 0;
			}
			zero_run = 0;
		}
		if (iv != 0) {
			if (!saw_nonzero)
				r.first_nonzero = pos;
			saw_nonzero = true;
			r.last_nonzero = pos;
		}
		prev = iv;
		have_prev = true;
		++pos;
	}
	if (zero_run > 0) {
		r.longest_zero_run = std::max(r.longest_zero_run, zero_run);
		r.long_zero_runs += zero_run >= 128U ? 1U : 0U;
		// A trailing run after the final audible sample is not an internal prompt gap.
		if (internal_zero_run > 0 && r.last_nonzero > 0 && (pcm.size() - zero_run) <= r.last_nonzero) {
			r.longest_internal_zero_run = std::max(r.longest_internal_zero_run, internal_zero_run);
			r.long_internal_zero_runs += internal_zero_run >= 128U ? 1U : 0U;
		}
	}
	if (saw_nonzero && r.last_nonzero >= r.first_nonzero) {
		std::size_t quiet_run = 0;
		for (std::size_t i = r.first_nonzero; i <= r.last_nonzero; ++i) {
			if (std::abs(int(pcm[i])) < 512) {
				++quiet_run;
			} else if (quiet_run > 0) {
				r.longest_quiet_run = std::max(r.longest_quiet_run, quiet_run);
				r.long_quiet_runs += quiet_run >= 128U ? 1U : 0U;
				quiet_run = 0;
			}
		}
		if (quiet_run > 0) {
			r.longest_quiet_run = std::max(r.longest_quiet_run, quiet_run);
			r.long_quiet_runs += quiet_run >= 128U ? 1U : 0U;
		}
	}
	if (!pcm.empty()) {
		r.dc = double(sum / pcm.size());
		r.rms = std::sqrt(double(sum2 / pcm.size()));
	}
}

bool decode_reference_flat(std::vector<std::uint8_t> const &rom, unsigned phrase, unsigned bank, char const *rom_label,
	std::filesystem::path const &wav_path, std::ofstream &trace, decode_report &r)
{
	r.name = "archive_reference_flat";
	r.wav = wav_path.filename().string();
	r.sample_rate = 8421;
	if (5U + 2U * phrase + 1U >= rom.size()) {
		r.warnings = "directory_oob";
		return false;
	}
	std::uint32_t index = ((std::uint32_t(rom[5U + 2U * phrase]) << 8) | rom[6U + 2U * phrase]) * 2U;
	r.start_offset = index + 1U;
	index = r.start_offset;
	trace << "{\"event\":\"directory_lookup\",\"decoder\":\"" << r.name << "\",\"phrase\":" << phrase
		  << ",\"bank\":" << bank << ",\"rom\":\"" << rom_label << "\",\"offset\":\"" << hex_off(index) << "\"}\n";
	std::vector<std::uint8_t> packed;
	for (unsigned guard = 0; guard < 0x20000U && index < rom.size(); ++guard) {
		std::uint32_t const op_off = index;
		std::uint8_t const op = rom[index++];
		if (r.blocks == 0)
			r.first_header = op;
		if (op == 0x00U)
			break;
		int const state_before = 0;
		if (op & 0x40U) {
			if (r.first_block_type.empty())
				r.first_block_type = "fixed_256";
			for (unsigned i = 0; i < 128U && index < rom.size(); ++i)
				packed.push_back(rom[index++]);
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "fixed_256", 8421, 256,
				state_before, state_before, "archive branch: op&0x40 before repeat handling");
			++r.blocks;
		} else if ((op & 0xc0U) == 0U) {
			if (r.first_block_type.empty())
				r.first_block_type = "silence_ignored";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "silence_ignored", 8421, 0,
				state_before, state_before, "archive decoder ignores silence duration");
			++r.blocks;
		} else if (op & 0x80U) {
			if (index >= rom.size())
				break;
			unsigned count = unsigned(rom[index++]) + 1U;
			for (unsigned i = 0; i < (count >> 1) && index < rom.size(); ++i)
				packed.push_back(rom[index++]);
			if (r.first_block_type.empty())
				r.first_block_type = "variable";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "variable", 8421, count,
				state_before, state_before, "archive variable block");
			++r.blocks;
		}
	}
	std::vector<std::int16_t> pcm;
	int state = 0;
	int sample = 0;
	for (std::uint8_t b : packed) {
		append_sample(pcm, std::uint8_t(b >> 4), 1, state, sample);
		append_sample(pcm, std::uint8_t(b & 0x0fU), 1, state, sample);
	}
	r.ok = !pcm.empty();
	compute_metrics(r, pcm);
	write_wav(wav_path, pcm, r.sample_rate);
	return r.ok;
}

bool decode_emulator_timed(std::vector<std::uint8_t> const &rom, unsigned phrase, unsigned bank, char const *rom_label,
	std::filesystem::path const &wav_path, std::ofstream &trace, decode_report &r)
{
	r.name = "emulator_timed";
	r.wav = wav_path.filename().string();
	r.sample_rate = 160000;
	if (5U + 2U * phrase + 1U >= rom.size()) {
		r.warnings = "directory_oob";
		return false;
	}
	std::uint32_t index = ((std::uint32_t(rom[5U + 2U * phrase]) << 8) | rom[6U + 2U * phrase]) * 2U + 1U;
	r.start_offset = index;
	trace << "{\"event\":\"directory_lookup\",\"decoder\":\"" << r.name << "\",\"phrase\":" << phrase
		  << ",\"bank\":" << bank << ",\"rom\":\"" << rom_label << "\",\"offset\":\"" << hex_off(index) << "\"}\n";
	std::vector<std::int16_t> pcm;
	int state = 0;
	int sample = 0;
	bool first_valid = false;
	for (unsigned guard = 0; guard < 0x20000U && index < rom.size(); ++guard) {
		std::uint32_t const op_off = index;
		std::uint8_t const op = rom[index++];
		if (r.blocks == 0)
			r.first_header = op;
		if (op == 0x00U && first_valid)
			break;
		first_valid = first_valid || op != 0x00U;
		int const state_before = state;
		switch (op & 0xc0U) {
		case 0x00U:
			sample = 0;
			state = 0;
			for (unsigned i = 0; i < 256U * (unsigned(op & 0x3fU) + 1U); ++i)
				pcm.push_back(0);
			if (r.first_block_type.empty())
				r.first_block_type = "silence";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "silence", 160000,
				256U * (unsigned(op & 0x3fU) + 1U), state_before, state, "MAME-style silence block");
			break;
		case 0x40U: {
			unsigned const divider = unsigned(op & 0x3fU) + 1U;
			for (unsigned i = 0; i < 128U && index < rom.size(); ++i) {
				std::uint8_t const b = rom[index++];
				append_sample(pcm, std::uint8_t(b >> 4), divider, state, sample);
				append_sample(pcm, std::uint8_t(b & 0x0fU), divider, state, sample);
			}
			if (r.first_block_type.empty())
				r.first_block_type = "fixed_256";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "fixed_256",
				160000 / divider, 256, state_before, state, "MAME-style fixed block");
			break;
		}
		case 0x80U: {
			if (index >= rom.size())
				break;
			unsigned const divider = unsigned(op & 0x3fU) + 1U;
			unsigned nibbles = unsigned(rom[index++]) + 1U;
			unsigned const requested = nibbles;
			while (nibbles > 0U && index < rom.size()) {
				std::uint8_t const b = rom[index++];
				append_sample(pcm, std::uint8_t(b >> 4), divider, state, sample);
				--nibbles;
				if (nibbles > 0U) {
					append_sample(pcm, std::uint8_t(b & 0x0fU), divider, state, sample);
					--nibbles;
				}
			}
			if (r.first_block_type.empty())
				r.first_block_type = "variable";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "variable",
				160000 / divider, requested, state_before, state, "MAME-style variable block");
			break;
		}
		case 0xc0U:
			if (index + 1 >= rom.size())
				break;
			{
				unsigned const repeats = unsigned(op & 0x07U) + 1U;
				unsigned const divider = unsigned(rom[index++] & 0x3fU) + 1U;
				unsigned const nibbles = unsigned(rom[index++]) + 1U;
				std::uint32_t const data_offset = index;
				for (unsigned repeat = 0; repeat < repeats; ++repeat) {
					std::uint32_t data = data_offset;
					unsigned left = nibbles;
					while (left > 0U && data < rom.size()) {
						std::uint8_t const b = rom[data++];
						append_sample(pcm, std::uint8_t(b >> 4), divider, state, sample);
						--left;
						if (left > 0U) {
							append_sample(pcm, std::uint8_t(b & 0x0fU), divider, state, sample);
							--left;
						}
					}
				}
				index += (nibbles + 1U) >> 1;
				if (r.first_block_type.empty())
					r.first_block_type = "repeat_loop";
				trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "repeat_loop",
					160000 / divider, nibbles * repeats, state_before, state,
					"MAME-comment repeat block with inline rate/count");
			}
			break;
		}
		++r.blocks;
	}
	r.ok = !pcm.empty();
	compute_metrics(r, pcm);
	write_wav(wav_path, pcm, r.sample_rate);
	return r.ok;
}

bool decode_board_aligned_variant(std::vector<std::uint8_t> const &rom, unsigned phrase, unsigned bank, char const *rom_label,
	std::filesystem::path const &wav_path, std::ofstream &trace, decode_report &r, char const *name,
	bool lsn_first, bool flip_sign_bit)
{
	r.name = name;
	r.wav = wav_path.filename().string();
	r.sample_rate = 8421;
	if (5U + 2U * phrase + 1U >= rom.size()) {
		r.warnings = "directory_oob";
		return false;
	}
	std::uint32_t index = ((std::uint32_t(rom[5U + 2U * phrase]) << 8) | rom[6U + 2U * phrase]) * 2U + 1U;
	r.start_offset = index;
	trace << "{\"event\":\"directory_lookup\",\"decoder\":\"" << r.name << "\",\"phrase\":" << phrase
		  << ",\"bank\":" << bank << ",\"rom\":\"" << rom_label << "\",\"offset\":\"" << hex_off(index)
		  << "\",\"nibble_order\":\"" << (lsn_first ? "lsn_msn" : "msn_lsn")
		  << "\",\"flip_sign_bit\":" << (flip_sign_bit ? "true" : "false") << "}\n";
	std::vector<std::int16_t> pcm;
	int state = 0;
	int sample = 0;
	for (unsigned guard = 0; guard < 0x20000U && index < rom.size(); ++guard) {
		std::uint32_t const op_off = index;
		std::uint8_t const op = rom[index++];
		if (r.blocks == 0)
			r.first_header = op;
		int const state_before = state;
		if (op == 0x00U)
			break;
		if (op & 0x40U) {
			for (unsigned i = 0; i < 128U && index < rom.size(); ++i) {
				std::uint8_t const b = rom[index++];
				append_byte_nibbles(pcm, b, 1, state, sample, lsn_first, flip_sign_bit);
			}
			if (r.first_block_type.empty())
				r.first_block_type = "fixed_256";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "fixed_256",
				8421, 256, state_before, state, "archive-aligned fixed block at native stream rate");
			++r.blocks;
		} else if ((op & 0xc0U) == 0U) {
			if (r.first_block_type.empty())
				r.first_block_type = "silence_ignored";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "silence_ignored", 8421,
				0, state_before, state, "archive-aligned control byte ignored");
			++r.blocks;
		} else if (op & 0x80U) {
			if (index >= rom.size())
				break;
			unsigned const requested = unsigned(rom[index++]) + 1U;
			unsigned pairs = requested >> 1;
			for (unsigned i = 0; i < pairs && index < rom.size(); ++i) {
				std::uint8_t const b = rom[index++];
				append_byte_nibbles(pcm, b, 1, state, sample, lsn_first, flip_sign_bit);
			}
			if (r.first_block_type.empty())
				r.first_block_type = "variable";
			trace_block(trace, r.name.c_str(), phrase, bank, rom_label, op_off, op, "variable",
				8421, pairs * 2U, state_before, state, "archive-aligned variable block");
			++r.blocks;
		}
	}
	r.ok = !pcm.empty();
	compute_metrics(r, pcm);
	write_wav(wav_path, pcm, r.sample_rate);
	return r.ok;
}

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

void write_report(std::filesystem::path const &path, unsigned phrase, unsigned bank, char const *rom_label,
	std::vector<decode_report> const &reports)
{
	std::ofstream o(path);
	o << "{\n  \"schema_version\":\"coinline.voiceware_decoder_comparison/v1\",\n"
	  << "  \"phrase\":\"" << hex_byte(std::uint8_t(phrase)) << "\",\n"
	  << "  \"bank\":" << bank << ",\n"
	  << "  \"rom\":" << q(rom_label) << ",\n  \"decoders\":[\n";
	for (std::size_t i = 0; i < reports.size(); ++i) {
		auto const &r = reports[i];
		o << "    {\"decoder_path\":" << q(r.name) << ",\"wav\":" << q(r.wav)
		  << ",\"ok\":" << (r.ok ? "true" : "false")
		  << ",\"non_silent\":" << (r.non_silent ? "true" : "false")
		  << ",\"offset\":\"" << hex_off(r.start_offset) << "\",\"first_header\":\"" << hex_byte(r.first_header)
		  << "\",\"first_block_type\":" << q(r.first_block_type) << ",\"sample_rate\":" << r.sample_rate
		  << ",\"duration\":" << std::fixed << std::setprecision(6)
		  << (r.sample_rate ? double(r.samples) / double(r.sample_rate) : 0.0)
		  << ",\"peak\":" << r.peak << ",\"rms\":" << r.rms << ",\"dc_offset\":" << r.dc
		  << ",\"zero_crossings\":" << r.zero_crossings << ",\"clipped_samples\":" << r.clipped
		  << ",\"longest_zero_run\":" << r.longest_zero_run << ",\"long_zero_runs\":" << r.long_zero_runs
		  << ",\"longest_internal_zero_run\":" << r.longest_internal_zero_run
		  << ",\"long_internal_zero_runs\":" << r.long_internal_zero_runs
		  << ",\"longest_quiet_run_lt512\":" << r.longest_quiet_run
		  << ",\"long_quiet_runs_lt512\":" << r.long_quiet_runs
		  << ",\"first_nonzero_sample\":" << r.first_nonzero << ",\"last_nonzero_sample\":" << r.last_nonzero
		  << ",\"block_count\":" << r.blocks << ",\"quality_heuristic\":"
		  << q((r.non_silent && r.peak > 256 && r.clipped == 0) ? "speech_or_tone_plausible" : "noise_or_silence_risk")
		  << ",\"failure_notes\":" << q(r.warnings) << "}";
		o << (i + 1U == reports.size() ? "\n" : ",\n");
	}
	o << "  ]\n}\n";
}

decode_report analyze_wav(std::filesystem::path const &path)
{
	decode_report r;
	r.name = "wav_analyzer";
	r.wav = path.string();
	std::ifstream f(path, std::ios::binary);
	if (!f)
		return r;
	std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(f)), {});
	if (b.size() < 44 || std::string(reinterpret_cast<char *>(b.data()), 4) != "RIFF") {
		r.warnings = "not_riff_wav";
		return r;
	}
	auto le16 = [&](std::size_t p) { return std::uint16_t(b[p] | (std::uint16_t(b[p + 1]) << 8)); };
	auto le32 = [&](std::size_t p) {
		return std::uint32_t(b[p] | (std::uint32_t(b[p + 1]) << 8) | (std::uint32_t(b[p + 2]) << 16) |
			(std::uint32_t(b[p + 3]) << 24));
	};
	r.sample_rate = le32(24);
	std::uint16_t const bits = le16(34);
	std::size_t data = 12;
	while (data + 8 <= b.size()) {
		std::uint32_t const sz = le32(data + 4);
		if (std::string(reinterpret_cast<char *>(&b[data]), 4) == "data") {
			data += 8;
			std::vector<std::int16_t> pcm;
			if (bits == 16) {
				for (std::size_t p = data; p + 1 < b.size() && p < data + sz; p += 2)
					pcm.push_back(std::int16_t(le16(p)));
			}
			r.ok = true;
			compute_metrics(r, pcm);
			return r;
		}
		data += 8 + sz;
	}
	r.warnings = "missing_data_chunk";
	return r;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc >= 3 && std::string(argv[1]) == "--analyze-wav") {
		auto r = analyze_wav(argv[2]);
		std::cout << "{\"schema_version\":\"coinline.voiceware_wav_analysis/v1\",\"wav\":" << q(r.wav)
			  << ",\"ok\":" << (r.ok ? "true" : "false") << ",\"sample_rate\":" << r.sample_rate
			  << ",\"duration\":" << (r.sample_rate ? double(r.samples) / double(r.sample_rate) : 0.0)
			  << ",\"peak\":" << r.peak << ",\"rms\":" << r.rms << ",\"dc_offset\":" << r.dc
			  << ",\"zero_crossings\":" << r.zero_crossings << ",\"clipped_samples\":" << r.clipped
			  << ",\"longest_zero_run\":" << r.longest_zero_run << ",\"long_zero_runs\":" << r.long_zero_runs
			  << ",\"longest_internal_zero_run\":" << r.longest_internal_zero_run
			  << ",\"long_internal_zero_runs\":" << r.long_internal_zero_runs
			  << ",\"longest_quiet_run_lt512\":" << r.longest_quiet_run
			  << ",\"long_quiet_runs_lt512\":" << r.long_quiet_runs
			  << ",\"first_nonzero_sample\":" << r.first_nonzero << ",\"last_nonzero_sample\":" << r.last_nonzero
			  << ",\"non_silent\":" << (r.non_silent ? "true" : "false") << "}\n";
		return r.ok ? 0 : 1;
	}
	if (argc < 6) {
		std::cerr << "usage: voiceware_decoder_sweep <u16.bin> <u26.bin> <outdir> <phrase_hex> <bank_dec>\n"
				  << "       voiceware_decoder_sweep --analyze-wav <wav>\n";
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
	trace << "{\"event\":\"phrase_start\",\"phrase\":" << phrase << ",\"bank\":" << bank
		  << ",\"rom\":\"" << rom_label << "\"}\n";
	std::vector<decode_report> reports;
	decode_report a;
	decode_reference_flat(rom, phrase, bank, rom_label, outdir / "phrase-0x3f-archive-reference-flat.wav", trace, a);
	reports.push_back(a);
	decode_report b;
	decode_emulator_timed(rom, phrase, bank, rom_label, outdir / "phrase-0x3f-emulator-timed.wav", trace, b);
	reports.push_back(b);
	decode_report c;
	decode_board_aligned_variant(rom, phrase, bank, rom_label, outdir / "phrase-0x3f-board-msn-raw.wav", trace, c,
		"board_msn_raw", false, false);
	reports.push_back(c);
	decode_report d;
	decode_board_aligned_variant(rom, phrase, bank, rom_label, outdir / "phrase-0x3f-board-lsn-raw.wav", trace, d,
		"board_lsn_raw", true, false);
	reports.push_back(d);
	decode_report e;
	decode_board_aligned_variant(rom, phrase, bank, rom_label, outdir / "phrase-0x3f-board-msn-signflip-raw.wav",
		trace, e, "board_msn_signflip_raw", false, true);
	reports.push_back(e);
	trace << "{\"event\":\"phrase_end\",\"phrase\":" << phrase << ",\"bank\":" << bank << "}\n";
	write_report(outdir / "voiceware-decoder-comparison.json", phrase, bank, rom_label, reports);
	return 0;
}
