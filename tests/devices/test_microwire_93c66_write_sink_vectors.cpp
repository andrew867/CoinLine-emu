// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_microwire_93c66.h"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

void pulse(millennium_microwire_93c66 &eeprom, bool di)
{
	eeprom.notify_port_b_clock(0x00U, 0x80U, di);
	eeprom.notify_port_b_clock(0x80U, 0x00U, di);
}

void command(millennium_microwire_93c66 &eeprom, unsigned body, unsigned body_bits)
{
	eeprom.set_chip_select(true);
	pulse(eeprom, false); // Idle clock before START.
	pulse(eeprom, true);
	for (unsigned bit = body_bits; bit > 0; --bit)
		pulse(eeprom, ((body >> (bit - 1U)) & 1U) != 0U);
}

void write_word(millennium_microwire_93c66 &eeprom, unsigned address, unsigned address_bits,
	std::uint16_t data)
{
	command(eeprom, (1U << address_bits) | address, address_bits + 2U);
	for (unsigned bit = 16; bit > 0; --bit)
		pulse(eeprom, ((data >> (bit - 1U)) & 1U) != 0U);
	eeprom.set_chip_select(false);
}

std::uint32_t read_word(millennium_microwire_93c66 &eeprom, unsigned address, unsigned address_bits)
{
	command(eeprom, (2U << address_bits) | address, address_bits + 2U);
	std::uint32_t data = 0;
	for (unsigned bit = 0; bit < 17; ++bit) {
		data = (data << 1U) | (eeprom.serial_out() ? 1U : 0U);
		pulse(eeprom, false);
	}
	eeprom.set_chip_select(false);
	return data;
}

bool exercise(unsigned address_bits)
{
	millennium_microwire_93c66 eeprom;
	eeprom.set_command_body_bits(address_bits + 2U);
	unsigned writes = 0, rejected = 0, enabled = 0;
	eeprom.set_access_trace_cb([&](char const *op, std::uint16_t, std::uint16_t) {
		if (std::strcmp(op, "write") == 0)
			++writes;
		else if (std::strcmp(op, "write_rejected") == 0)
			++rejected;
		else if (std::strcmp(op, "ewen") == 0)
			++enabled;
	});

	// A disabled WRITE still consumes all sixteen data bits.
	write_word(eeprom, 0, address_bits, 0xcafeU);
	if (writes != 0 || rejected != 1 || enabled != 0 || read_word(eeprom, 0, address_bits) != 0xffffU)
		return false;

	command(eeprom, 3U << (address_bits - 2U), address_bits + 2U); // EWEN
	eeprom.set_chip_select(false);
	unsigned const high_address = (1U << (address_bits - 1U)) | 5U;
	write_word(eeprom, 5, address_bits, 0xabcdU);
	write_word(eeprom, high_address, address_bits, 0x1234U);
	if (writes != 2 || enabled != 1 || read_word(eeprom, 5, address_bits) != 0xabcdU ||
		read_word(eeprom, high_address, address_bits) != 0x1234U)
		return false;

	command(eeprom, 0, address_bits + 2U); // EWDS
	eeprom.set_chip_select(false);
	write_word(eeprom, 5, address_bits, 0xffffU);
	return writes == 2 && rejected == 2 && read_word(eeprom, 5, address_bits) == 0xabcdU;
}

} // namespace

int main()
{
	for (unsigned const address_bits : {6U, 8U}) {
		if (!exercise(address_bits)) {
			std::cerr << "Microwire write/read regression for " << address_bits << " address bits\n";
			return 1;
		}
	}
	return 0;
}
