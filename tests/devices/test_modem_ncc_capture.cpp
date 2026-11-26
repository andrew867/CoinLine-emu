// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_modem_ncc.h"
#include "millennium_sha256.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {

using bytes = std::vector<std::uint8_t>;
using coinline::modem::ncc::ncc_frame_fields;
using coinline::modem::ncc::decode;
using coinline::modem::ncc::encode;

int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

bool parse_wire(std::string const &hex, bytes &out)
{
	if (hex.size() % 2 || hex.size() < 12 || hex.size() > 512)
		return false;
	for (std::size_t i = 0; i < hex.size(); i += 2) {
		int const high = hex_digit(hex[i]);
		int const low = hex_digit(hex[i + 1]);
		if (high < 0 || low < 0)
			return false;
		out.push_back(static_cast<std::uint8_t>((high << 4) | low));
	}
	return true;
}

// Apply arbitrary TCP chunk boundaries to the original directional byte
// streams. The native runtime forwards bytes and does not run this parser.
bool replay_chunks(std::vector<bytes> const &expected, std::size_t chunk_size)
{
	bytes stream, pending;
	for (auto const &frame : expected)
		stream.insert(stream.end(), frame.begin(), frame.end());
	std::size_t found = 0;
	for (std::size_t offset = 0; offset < stream.size();) {
		std::size_t const end = std::min(offset + chunk_size, stream.size());
		pending.insert(pending.end(), stream.begin() + offset, stream.begin() + end);
		offset = end;
		while (pending.size() >= 3) {
			std::size_t const length = static_cast<std::size_t>(pending[2]) + 1U;
			if (pending[0] != 2U || length < 6U || length > 256U)
				return false;
			if (pending.size() < length)
				break;
			bytes const actual(pending.begin(), pending.begin() + length);
			ncc_frame_fields decoded{};
			std::string error;
			if (found >= expected.size() || actual != expected[found++] || !decode(actual, decoded, error)
				|| encode(decoded) != actual)
				return false;
			pending.erase(pending.begin(), pending.begin() + length);
		}
	}
	return pending.empty() && found == expected.size();
}

int fail(char const *why)
{
	std::cerr << why << '\n';
	return 1;
}

} // namespace

int main(int argc, char **argv)
{
	// A missing capture is a visible skip, not a passing generated fixture.
	if (argc > 2)
		return fail("usage: test_modem_ncc_capture [fixture-directory]");
	std::filesystem::path const base = argc == 2 ? std::filesystem::path(argv[1])
		: std::filesystem::path(COINLINE_EMU_SOURCE_DIR) / "fixtures/ncc/streams";
	std::string const capture_name = "native-session.tsv";
	std::error_code ec;
	bool const has_capture = std::filesystem::exists(base / capture_name, ec);
	if (ec)
		return fail("cannot check captured NCC fixture");
	bool const has_manifest = std::filesystem::exists(base / (capture_name + ".sha256"), ec);
	if (ec)
		return fail("cannot check captured NCC fixture manifest");
	if (!has_capture && !has_manifest) {
		std::cout << "SKIP: no qualified native NCC capture available\n";
		return 77;
	}
	std::ifstream capture(base / capture_name, std::ios::binary);
	std::ifstream manifest(base / (capture_name + ".sha256"));
	if (!capture || !manifest)
		return fail("captured NCC fixture or manifest missing");
	std::string const content{std::istreambuf_iterator<char>(capture), std::istreambuf_iterator<char>{}};
	std::string expected_hash, file_name;
	manifest >> expected_hash >> file_name;
	if (file_name != capture_name || expected_hash.size() != 64
		|| millennium_sha256_hex(reinterpret_cast<std::uint8_t const *>(content.data()), content.size()) != expected_hash)
		return fail("captured fixture digest mismatch");
	if (content.empty() || content.back() != '\n' || content.find('\r') != std::string::npos)
		return fail("captured fixture is not canonical LF-terminated text");
	std::istringstream input(content);
	std::string line;
	if (!std::getline(input, line) || line != "direction\thex")
		return fail("captured fixture header invalid");
	std::array<std::vector<bytes>, 2> directions;
	std::vector<std::array<unsigned, 3>> description;
	while (std::getline(input, line)) {
		if (line.size() < 3 || line[1] != '\t' || (line[0] != 'T' && line[0] != 'H'))
			return fail("captured fixture row invalid");
		unsigned const direction = line[0] == 'T' ? 0U : 1U;
		bytes frame;
		ncc_frame_fields parsed{};
		std::string error;
		if (!parse_wire(line.substr(2), frame) || !decode(frame, parsed, error) || encode(parsed) != frame)
			return fail("captured frame rejected or re-encoded differently");
		description.push_back({direction, parsed.control, parsed.payload.empty() ? 0U : parsed.payload[0]});
		directions[direction].push_back(std::move(frame));
	}
	// The exact expected direction/control/application sequence is checked
	// separately from CRC and framing so an incomplete exchange cannot pass.
	std::vector<std::array<unsigned, 3>> const alarm = {
		{0, 0x00, 7}, {1, 0x08, 0}, {1, 0x00, 16}, {0, 0x08, 0}, {0, 0x20, 0}
	};
	std::vector<std::array<unsigned, 3>> const routine = {
		{0, 0x00, 8}, {1, 0x08, 0}, {1, 0x00, 13}, {0, 0x08, 0}, {0, 0x20, 0}
	};
	std::vector<std::array<unsigned, 3>> const clock = {
		{0, 0x00, 36}, {1, 0x08, 0}, {1, 0x00, 20}, {0, 0x08, 0},
		{1, 0x01, 13}, {0, 0x09, 0}, {0, 0x20, 0}
	};
	if (description != alarm && description != routine && description != clock)
		return fail("captured session is not an expected complete dialogue");
	for (auto const &stream : directions)
		for (std::size_t const chunk : {1U, 2U, 7U, 64U, 256U})
			if (!replay_chunks(stream, chunk))
				return fail("captured byte-stream chunk replay mismatch");
	std::cout << "native NCC capture replays exactly in both directions\n";
	return 0;
}
