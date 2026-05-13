// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_mach_async.h"

#include <iostream>

int main()
{
	using namespace millennium_mach_async;
	if (k_g_epm_clock_mask != 0x38U)
		return 1;
	if (!h_async_path_enabled(0x07U))
		return 2;
	if (h_async_path_enabled(0x08U))
		return 3;
	if (d_device_addr(0xfeU) != 0x02U)
		return 4;
	if (g_epm_clock_select(0xf8U) != 0x38U)
		return 5;
	std::cout << "mach_async_masks ok\n";
	return 0;
}
