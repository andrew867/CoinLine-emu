// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_firmware.h"

#include "millennium_sha256.h"

#include <cctype>
#include <fstream>
#include <sstream>

bool millennium_read_file(std::string const &path, std::vector<std::uint8_t> &out, std::string &error)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		error = "millennium_read_file: cannot open: " + path;
		return false;
	}
	f.seekg(0, std::ios::end);
	auto const sz = f.tellg();
	if (sz < 0) {
		error = "millennium_read_file: tellg failed: " + path;
		return false;
	}
	f.seekg(0, std::ios::beg);
	out.resize(std::size_t(sz));
	if (!out.empty() && !f.read(reinterpret_cast<char *>(out.data()), std::streamsize(out.size()))) {
		error = "millennium_read_file: read incomplete: " + path;
		return false;
	}
	return true;
}

namespace {

bool parse_first_uint_after(std::string const &text, char const *key, std::size_t &out, std::string &error)
{
	auto p = text.find(key);
	if (p == std::string::npos) {
		error = std::string("parse_first_uint_after: key not found: ") + key;
		return false;
	}
	auto colon = text.find(':', p);
	if (colon == std::string::npos) {
		error = "parse_first_uint_after: colon not found";
		return false;
	}
	std::size_t i = colon + 1;
	while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
		++i;
	if (i >= text.size()) {
		error = "parse_first_uint_after: no digits";
		return false;
	}
	std::size_t v = 0;
	if (i + 2 < text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
		i += 2;
		while (i < text.size() && std::isxdigit(static_cast<unsigned char>(text[i]))) {
			int d = std::tolower(static_cast<unsigned char>(text[i]));
			v *= 16U;
			v += unsigned(d <= '9' ? d - '0' : 10 + (d - 'a'));
			++i;
		}
	} else {
		while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
			v = v * 10U + std::size_t(text[i] - '0');
			++i;
		}
	}
	out = v;
	return true;
}

bool parse_strict_hash(std::string const &text, bool &strict_out, std::string &error)
{
	auto p = text.find("\"strict_hash\"");
	if (p == std::string::npos) {
		error = "parse_strict_hash: strict_hash not found";
		return false;
	}
	auto colon = text.find(':', p);
	if (colon == std::string::npos) {
		error = "parse_strict_hash: colon not found";
		return false;
	}
	std::size_t i = colon + 1;
	while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
		++i;
	if (i + 4 <= text.size() && text.compare(i, 4, "true") == 0) {
		strict_out = true;
		return true;
	}
	if (i + 5 <= text.size() && text.compare(i, 5, "false") == 0) {
		strict_out = false;
		return true;
	}
	error = "parse_strict_hash: expected true/false";
	return false;
}

bool hash_allowed(std::string const &hashes_json, std::string const &sha256_hex)
{
	// Look for "sha256_hex": "<same>" or "sha256_hex":"<same>"
	std::string needle = "\"sha256_hex\"";
	for (std::size_t pos = 0;;) {
		auto p = hashes_json.find(needle, pos);
		if (p == std::string::npos)
			return false;
		auto colon = hashes_json.find(':', p);
		if (colon == std::string::npos)
			return false;
		auto q1 = hashes_json.find('"', colon);
		if (q1 == std::string::npos)
			return false;
		auto q2 = hashes_json.find('"', q1 + 1);
		if (q2 == std::string::npos)
			return false;
		std::string val = hashes_json.substr(q1 + 1, q2 - q1 - 1);
		if (val == sha256_hex)
			return true;
		pos = q2 + 1;
	}
}

} // namespace

bool millennium_board_parse_rom_size(std::string const &board_json, std::size_t &rom_size_out,
	std::string &error)
{
	return parse_first_uint_after(board_json, "\"rom_size\"", rom_size_out, error);
}

bool millennium_board_parse_strict_hash(std::string const &board_json, bool &strict_out,
	std::string &error)
{
	return parse_strict_hash(board_json, strict_out, error);
}

millennium_firmware_validate_result millennium_validate_firmware(std::vector<std::uint8_t> const &bytes,
	std::string const &board_json_text, std::string const &firmware_hashes_json_text)
{
	millennium_firmware_validate_result r;
	std::string err;
	if (!millennium_board_parse_rom_size(board_json_text, r.rom_size_expected, err)) {
		r.error = err;
		return r;
	}
	if (bytes.size() != r.rom_size_expected) {
		std::ostringstream os;
		os << "firmware size mismatch: expected " << r.rom_size_expected << " bytes, got " << bytes.size();
		r.error = os.str();
		return r;
	}
	if (!millennium_board_parse_strict_hash(board_json_text, r.strict_hash, err)) {
		r.error = err;
		return r;
	}
	r.sha256_hex = millennium_sha256_hex(bytes.data(), bytes.size());
	if (r.strict_hash) {
		if (!hash_allowed(firmware_hashes_json_text, r.sha256_hex)) {
			r.error = "firmware hash mismatch: SHA-256 not listed in firmware-hashes.json";
			return r;
		}
	}
	r.ok = true;
	return r;
}
