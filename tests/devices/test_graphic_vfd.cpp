// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_graphic_vfd.h"

#include <cstdint>
#include <iostream>

int main()
{
	millennium_graphic_vfd vfd;
	auto check = [](bool value, char const *reason) {
		if (!value)
			std::cerr << reason << '\n';
		return value;
	};
	vfd.configure(5, 20, true);
	vfd.reset();
	vfd.write_pixel(0x1280, 0xff, 0);
	if (!check(vfd.read_status(0xad, 0) == 0xac && vfd.byte(0, 0) == 0 && !vfd.lit(0, 87),
		"Reset must hold display inactive and preserve independent status bits"))
		return 1;
	vfd.release_reset();
	vfd.write_control(0x13, 1);
	if (!check(vfd.read_status(0xac, 1) == 0xad, "Reset release must assert ready"))
		return 1;
	for (unsigned page = 0; page < 4; ++page) {
		std::uint16_t base = 0x1280 + (page & 1) * 0x800;
		vfd.write_control(0x13 | ((page & 2) << 5), 2);
		vfd.write_pixel(base, 0x80 | page, 3);
		vfd.write_pixel(base + 0x07f, 0x11 + page, 3);
		vfd.write_pixel(base + 0x080, 0x22 + page, 3);
		vfd.write_pixel(base + 0x500, 0x01 << page, 3);
		vfd.write_pixel(base + 0x57f, 0x44 + page, 3);
	}
	if (!check(vfd.read_status(0xac, 7) == 0xac && vfd.read_status(0xac, 8) == 0xad,
		"Configurable write busy interval changed unrelated status bits"))
		return 1;
	for (unsigned page = 0; page < 4; ++page) {
		if (!check(vfd.byte(page, 0) == (0x80 | page) && vfd.byte(page, 0x07f) == 0x11 + page
			&& vfd.byte(page, 0x080) == 0x22 + page && vfd.byte(page, 0x500) == (1U << page)
			&& vfd.byte(page, 0x57f) == 0x44 + page, "Write windows or band boundaries aliased"))
			return 1;
	}
	// Last write selected page 3, but page 0 is still visible.
	if (!check(vfd.visible_page() == 0 && vfd.write_page() == 3 && vfd.lit(0, 0)
		&& !vfd.lit(0, 1) && vfd.lit(0, 87) && !vfd.lit(126, 0), "Offscreen writes or orientation incorrect"))
		return 1;
	vfd.write_control(0x5f, 9);
	if (!check(vfd.visible_page() == 3 && vfd.lit(0, 3) && !vfd.lit(0, 0),
		"Visible-page selection must be independent of writes"))
		return 1;
	vfd.write_control(0x5d, 10);
	vfd.write_pixel(0x1a80, 0x77, 10);
	vfd.write_control(0x5f, 11);
	if (!check(vfd.brightness() == 0x77 && vfd.byte(3, 0) == 0x83, "Brightness corrupted base pixel"))
		return 1;
	vfd.write_control(0x5e, 12);
	vfd.write_control(0x5f, 13);
	if (!check(vfd.byte(3, 0) == 0 && vfd.byte(3, 0x57f) == 0
		&& vfd.byte(2, 0) == 0x82 && vfd.byte(0, 0) == 0x80
		&& vfd.read_status(0x80, 31) == 0x80 && vfd.read_status(0x80, 32) == 0x81,
		"Clear must affect only the selected write page"))
		return 1;
	vfd.write_control(0x03, 33);
	if (!check(!vfd.lit(0, 0) && vfd.byte(0, 0x500) == 1, "Display disable erased memory"))
		return 1;
	vfd.write_control(0x33, 34);
	if (!check(vfd.lit(125, 87) && vfd.byte(0, 125) == 0, "Lamp test altered backing RAM"))
		return 1;
	vfd.write_control(0x13, 35);
	for (unsigned group = 0; group < 3; ++group) {
		vfd.write_keys(0x80 | (1U << group));
		if (!check(vfd.read_keys(0x85, 0) == 0xfd, "Idle key inputs or independent bits incorrect"))
			return 1;
		for (unsigned key = 0; key < 12; ++key) {
			unsigned const expected = key % 3 == group ? (0x78 & ~(1U << (key / 3 + 3))) : 0x78;
			if (!check((vfd.read_keys(0x85, 1U << key) & 0x78) == expected, "Physical key scan mapping incorrect"))
				return 1;
		}
	}
	vfd.write_keys(1);
	if (!check((vfd.read_keys(0, 0x11) & 0x78) == 0x70
		&& (vfd.read_keys(0, 0x11) & 0x78) == 0x70, "Held key changed without physical release"))
		return 1;
	vfd.write_keys(2);
	if (!check((vfd.read_keys(0, 0x11) & 0x78) == 0x68
		&& (vfd.read_keys(0, 0x01) & 0x78) == 0x78, "Concurrent raw switch states or release incorrect"))
		return 1;
	return 0;
}
