// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad_model.h"

#include <iostream>

namespace {

std::uint8_t pb_for_column(int col) noexcept
{
	return static_cast<std::uint8_t>(~(1U << col) & 0xff);
}

} // namespace

int main()
{
	millennium_keypad_model m;
	m.reset();

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			int const bit = millennium_keypad_model::matrix_row_col_to_keymask_bit(row, col);
			if (bit < 0) {
				std::cerr << "bad bit index\n";
				return 1;
			}
			std::uint32_t const km = 1U << unsigned(bit);
			m.write_port_b(pb_for_column(col), 0);
			std::uint8_t const pa = m.read_port_a(km, 0);
			std::uint8_t const expect = static_cast<std::uint8_t>(0x0fU & ~(1U << row));
			if (pa != expect) {
				std::cerr << "matrix mismatch row " << row << " col " << col << " got 0x" << int(pa) << " expect 0x"
					  << int(expect) << "\n";
				return 1;
			}
		}
	}
	return 0;
}
