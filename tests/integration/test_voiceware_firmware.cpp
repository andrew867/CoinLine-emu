// SPDX-License-Identifier: GPL-2.0-or-later
// Class C: full firmware proof belongs in CI with coinline-mame.exe + real flash.bin.
// This binary exits 0 when the emulator executable is absent (explicit skip for dev boxes).

#include <filesystem>
#include <iostream>

namespace {

std::filesystem::path emu_root() { return std::filesystem::path(COINLINE_EMU_SOURCE_DIR); }

} // namespace

int main()
{
	auto const exe = emu_root() / "build/bin/coinline-mame.exe";
	if (!std::filesystem::exists(exe)) {
		std::cout << "SKIP: coinline-mame.exe not built — Class C firmware assertion deferred to CI\n";
		return 77;
	}
	std::cout << "SKIP: run tools/windows/test-coinline-emulator.ps1 with firmware to assert voiceware JSONL\n";
	return 77;
}
