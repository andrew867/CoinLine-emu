// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_flash_model.h"
#include "millennium_mach_pio.h"

#include <cstdio>

static void command(millennium_flash_chip &chip, std::uint8_t value, std::uint64_t now)
{
	chip.write(0x555, 0xaa, now);
	chip.write(0x2aa, 0x55, now);
	chip.write(0x555, value, now);
}

int main()
{
	millennium_flash_chip chip;
	chip.bytes.fill(0xff);
	chip.write(0x1234, 0x11, 0);
	if (chip.read(0x1234, 0) != 0xff)
		return 1;
	command(chip, 0x90, 0);
	if (chip.read(0, 0) != 0x20 || chip.read(1, 0) != 0xe2)
		return 2;
	chip.write(0, 0xf0, 0);
	command(chip, 0xa0, 1);
	chip.write(0x1234, 0x85, 1);
	auto const first = chip.read(0x1234, 2);
	auto const second = chip.read(0x1234, 3);
	if ((first & 0x80) || ((first ^ second) & 0x40) == 0 || chip.read(0x1234, 9) != 0x85)
		return 3;
	command(chip, 0xa0, 10);
	chip.write(0x1234, 0xff, 10);
	chip.complete(18);
	if (chip.bytes[0x1234] != 0x85 || (chip.read(0x1234, 18) & 0x20) == 0)
		return 4;
	chip.write(0, 0xf0, 19);
	chip.bytes[0x10000] = 0;
	chip.bytes[0x20000] = 0;
	command(chip, 0x80, 20);
	chip.write(0x555, 0xaa, 20);
	chip.write(0x2aa, 0x55, 20);
	chip.write(0, 0x30, 20);
	chip.write(0x10000, 0x30, 35);
	if (chip.read(0x1234, 36) & 0x08)
		return 5;
	if ((chip.read(0x1234, 90) & 0x08) == 0)
		return 6;
	chip.complete(600085);
	if (chip.read(0x1234, 600085) != 0xff || chip.read(0x10000, 600085) != 0xff
		|| chip.read(0x20000, 600085) != 0)
		return 7;
	command(chip, 0x20, 600086);
	chip.write(0, 0xa0, 600086);
	chip.write(0x4010, 0x42, 600086);
	chip.complete(600094);
	chip.write(0, 0x90, 600095);
	chip.write(0, 0, 600095);
	if (chip.read(0x4010, 600095) != 0x42)
		return 8;
	if (millennium_mach_flash_address(0, 0x1f) != 0xc001f
		|| millennium_mach_flash_address(1, 0x1f) != 0xd001f
		|| millennium_mach_flash_address(2, 0x1f) != 0xe001f
		|| millennium_mach_flash_address(3, 0x1f) != 0xf001f
		|| millennium_mach_flash_address(6, 0x1f) != 0x1f
		|| millennium_mach_flash_address(7, 0x1f) != 0x1f
		|| millennium_mach_flash_address(0, 0x10000) != 0x10000)
		return 9;
	std::puts("Flash command timing, NOR programming and remap passed");
	return 0;
}
