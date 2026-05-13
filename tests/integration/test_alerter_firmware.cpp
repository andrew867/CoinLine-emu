// SPDX-License-Identifier: GPL-2.0-or-later
// Class C: firmware-level alerter trace checks belong in CI with coinline-mame.exe + flash.bin.
// Exits 77 when prerequisites are missing so local runs never false-pass.

#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	auto const exe = emu_root() / "build/bin/coinline-mame.exe";
	auto const fw = emu_root().parent_path() / "firmware" / "flash.bin";
	if (!std::filesystem::exists(exe)) {
		std::cout << "SKIP: coinline-mame.exe not built\n";
		return 77;
	}
	if (!std::filesystem::exists(fw)) {
		std::cout << "SKIP: firmware flash.bin not present\n";
		return 77;
	}
	std::cout << "SKIP: Class C alerter JSONL correlation — run tools/windows/test-coinline-emulator.ps1 with firmware\n";
	return 77;
}
