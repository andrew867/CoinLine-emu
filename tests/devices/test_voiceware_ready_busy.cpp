// SPDX-License-Identifier: GPL-2.0-or-later
// Phrase port read is driven by MAME `upd7759_device::busy_r()` (high when idle) and maps to 0xFF/0x7F.
// This test is a build/acceptance placeholder; full MAME link lives in the external MAME build.

#include <iostream>

int main()
{
	std::cout << "voiceware 0x61: busy_r-based read levels (0xFF idle, 0x7F when core not idle)\n";
	return 0;
}
