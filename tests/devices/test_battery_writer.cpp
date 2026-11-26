// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_battery_writer.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
using bytes = std::vector<std::uint8_t>;

bytes read(std::filesystem::path const &path)
{
	std::ifstream in(path, std::ios::binary);
	return bytes(std::istreambuf_iterator<char>(in), {});
}

int fail(char const *why)
{
	std::cerr << why << '\n';
	return 1;
}
}

int main()
{
	using result = millennium_battery_writer::result;
	auto const dir = std::filesystem::current_path() / ("battery-writer-" +
		std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	auto const path = dir / "battery";
	auto const tmp = dir / "battery.tmp";
	bytes const first(524822U, 0x11), second(524822U, 0x22), final(524822U, 0x33);
	millennium_battery_writer writer;
	if (writer.poll() != result::idle || !writer.write_atomic(path, first) || read(path) != first)
		return fail("initial battery snapshot failed");
	if (!writer.submit(path, second) || writer.submit(path, final))
		return fail("writer must have at most one pending immutable snapshot");
	if (writer.finish() != result::saved || read(path) != second || writer.poll() != result::idle)
		return fail("async snapshot did not persist unchanged");
	// A failing temporary-file write must leave the previous complete image
	// untouched. The next attempt is allowed to recover.
	if (!std::filesystem::create_directory(tmp) || !writer.submit(path, final))
		return fail("could not set up write failure control");
	if (writer.finish() != result::failed || read(path) != second)
		return fail("failed write changed the last complete snapshot");
	if (!std::filesystem::remove(tmp) || !writer.submit(path, final) || writer.finish() != result::saved || read(path) != final)
		return fail("the writer did not recover after a temporary failure");
	// The final shutdown write replaces an already published image.
	if (!writer.write_atomic(path, first) || read(path) != first || std::filesystem::exists(tmp))
		return fail("final atomic replacement failed");
	std::filesystem::remove(path);
	std::filesystem::remove(dir);
	std::cout << "battery snapshot async, failed-write preservation, retry and final replacement passed\n";
	return 0;
}
