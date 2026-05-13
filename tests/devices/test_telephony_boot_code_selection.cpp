// SPDX-License-Identifier: GPL-2.0-or-later
// Boot-code selection vectors for telephony reset vs ACK paths.

#include <cstdint>
#include <iostream>

namespace {

std::uint8_t select_boot_code(bool tel_hook_onhook_stable)
{
	// Mirrors IPCODES POWER_ON_RESET vs POWER_ON_ACK boot injection (legacy host path).
	return tel_hook_onhook_stable ? 0x70U : 0x72U;
}

} // namespace

int main()
{
	if (select_boot_code(true) != 0x70U) {
		std::cerr << "expected POWER_ON_RESET 0x70 when on-hook stable\n";
		return 1;
	}
	if (select_boot_code(false) != 0x72U) {
		std::cerr << "expected POWER_ON_ACK 0x72 when off-hook (not stable on-hook)\n";
		return 2;
	}
	std::cout << "telephony_boot_code_selection ok\n";
	return 0;
}
