// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem_model.h"

#include <cstdint>
#include <iostream>

namespace {

std::uint8_t read_detect_register(millennium_modem_model &modem)
{
	constexpr std::uint8_t address = 0x20U;
	constexpr std::uint8_t read_inactive = 0x04U;
	constexpr std::uint8_t write_inactive = 0x08U;
	constexpr std::uint8_t clock = 0x02U;
	constexpr std::uint8_t read_active = address | write_inactive;

	modem.ssi_pio_a_write(read_active | read_inactive | clock);
	modem.ssi_pio_a_write(read_active | clock);
	std::uint8_t value = modem.ssi_mpd_level() ? 1U : 0U;
	for (unsigned bit = 1; bit < 8; ++bit) {
		modem.ssi_pio_a_write(read_active);
		modem.ssi_pio_a_write(read_active | clock);
		if (modem.ssi_mpd_level())
			value |= static_cast<std::uint8_t>(1U << bit);
	}
	modem.ssi_pio_a_write(read_active | read_inactive | clock);
	return value;
}

char write_register(millennium_modem_model &modem, std::uint8_t address, std::uint8_t value,
	bool extended_address = false)
{
	constexpr std::uint8_t clock = 0x02U;
	constexpr std::uint8_t read_inactive = 0x04U;
	constexpr std::uint8_t write_inactive = 0x08U;
	std::uint8_t const base = (address << 4) | read_inactive;
	for (unsigned bit = 0; bit < 8; ++bit) {
		std::uint8_t const inverted_data = ((value >> bit) & 1U) ^ 1U;
		std::uint8_t const level = base | write_inactive | inverted_data;
		modem.ssi_pio_a_write(level | clock, extended_address);
		modem.ssi_pio_a_write(level, extended_address);
	}
	modem.ssi_pio_a_write(base | clock, extended_address);
	return modem.ssi_pio_a_write(base | write_inactive | clock, extended_address);
}

} // namespace

int main()
{
	millennium_modem_model modem;
	modem.reset();
	modem.note_modem_relay(true, 0.0);
	if (read_detect_register(modem) != 0x00U) {
		std::cerr << "No line should leave the detect register clear\n";
		return 1;
	}
	modem.set_line_present(true);
	if (read_detect_register(modem) != 0x02U) {
		std::cerr << "The SSI read sequence did not return the dial tone bit\n";
		return 1;
	}
	modem.poll_ncc_link(30.0);
	if (modem.ncc_link_up() || modem.dcd() || read_detect_register(modem) != 0x02U) {
		std::cerr << "A line test should keep dial tone without opening a data call\n";
		return 1;
	}
	if (write_register(modem, 3, 0x1aU) != 0
		|| write_register(modem, 4, 0x02U, true) != 0
		|| write_register(modem, 0, 0x02U) != '0'
		|| write_register(modem, 0, 0x02U) != 0
		|| write_register(modem, 0, 0x00U) != 0
		|| write_register(modem, 0, 0x02U) != '0') {
		std::cerr << "DTMF should start when transmit is enabled and repeat only after a pause\n";
		return 1;
	}
	modem.note_modem_relay(false, 31.0);
	if (read_detect_register(modem) != 0x00U) {
		std::cerr << "A released modem should have an idle detect register\n";
		return 1;
	}
	return 0;
}
