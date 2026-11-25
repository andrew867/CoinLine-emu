// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_flash_model.h"

#include <algorithm>

namespace {
constexpr std::uint32_t unlock_address_1 = 0x555;
constexpr std::uint32_t unlock_address_2 = 0x2aa;
constexpr std::uint64_t byte_program_us = 8;
constexpr std::uint64_t block_select_us = 50;
constexpr std::uint64_t block_erase_us = 600000;
constexpr std::uint64_t chip_erase_us = 5000000;
}

void millennium_flash_chip::power_on()
{
	m_command = command::array;
	m_operation = operation::idle;
	m_bypass = false;
	m_program_error = false;
	m_toggle = 0;
}

void millennium_flash_chip::complete(std::uint64_t now_us)
{
	if (m_operation == operation::idle || now_us < m_until)
		return;
	if (m_operation == operation::program) {
		bytes[m_program_address] &= m_program_value;
		m_command = m_program_error ? command::error : m_bypass ? command::bypass : command::array;
	} else {
		for (unsigned block = 0; block < 8; ++block)
			if (m_erase_mask & (1U << block))
				std::fill_n(bytes.begin() + block * block_size, block_size, 0xff);
		m_command = command::array;
	}
	m_operation = operation::idle;
}

std::uint8_t millennium_flash_chip::read(std::uint32_t address, std::uint64_t now_us)
{
	address &= size - 1;
	complete(now_us);
	if (m_operation == operation::program || m_command == command::error) {
		m_toggle ^= 0x40;
		return std::uint8_t((~m_program_value & 0x80) | m_toggle
			| (m_command == command::error ? 0x20 : 0));
	}
	if (m_operation == operation::erase) {
		m_toggle ^= 0x44;
		return std::uint8_t((m_toggle & 0x40)
			| (m_erase_mask & (1U << (address / block_size)) ? m_toggle & 4 : 0)
			| (now_us >= m_erase_start ? 8 : 0));
	}
	if (m_command == command::autoselect) {
		switch (address & 0xff) {
		case 0: return 0x20;
		case 1: return 0xe2;
		case 2: return 0;
		default: return 0xff;
		}
	}
	return bytes[address];
}

void millennium_flash_chip::write(std::uint32_t address, std::uint8_t value, std::uint64_t now_us)
{
	address &= size - 1;
	complete(now_us);
	if (m_operation == operation::erase && now_us < m_erase_start && value == 0x30) {
		m_erase_mask |= std::uint8_t(1U << (address / block_size));
		m_erase_start = now_us + block_select_us;
		m_until = m_erase_start + block_erase_us;
		return;
	}
	if (m_operation != operation::idle)
		return;
	if (m_command == command::program || m_command == command::bypass_program) {
		m_program_address = address;
		m_program_value = value;
		m_program_error = (bytes[address] & value) != value;
		m_operation = operation::program;
		m_until = now_us + byte_program_us;
		m_toggle = 0;
		return;
	}
	if (value == 0xf0 && !m_bypass) {
		m_command = command::array;
		return;
	}
	std::uint32_t const command_address = address & 0x7ff;
	switch (m_command) {
	case command::array:
		if (command_address == unlock_address_1 && value == 0xaa)
			m_command = command::unlock;
		break;
	case command::unlock:
		m_command = command_address == unlock_address_2 && value == 0x55
			? command::select : command::array;
		break;
	case command::select:
		m_command = command::array;
		if (command_address != unlock_address_1)
			break;
		switch (value) {
		case 0x90: m_command = command::autoselect; break;
		case 0xa0: m_command = command::program; break;
		case 0x80: m_command = command::erase_unlock; break;
		case 0x20: m_bypass = true; m_command = command::bypass; break;
		}
		break;
	case command::erase_unlock:
		m_command = command_address == unlock_address_1 && value == 0xaa
			? command::erase_select : command::array;
		break;
	case command::erase_select:
		m_command = command_address == unlock_address_2 && value == 0x55
			? command::erase_confirm : command::array;
		break;
	case command::erase_confirm:
		m_command = command::array;
		if (value == 0x30 || (command_address == unlock_address_1 && value == 0x10)) {
			bool const chip = value == 0x10;
			m_erase_mask = chip ? 0xff : std::uint8_t(1U << (address / block_size));
			m_operation = operation::erase;
			m_erase_start = now_us + (chip ? 0 : block_select_us);
			m_until = m_erase_start + (chip ? chip_erase_us : block_erase_us);
			m_toggle = 0;
		}
		break;
	case command::bypass:
		if (value == 0xa0)
			m_command = command::bypass_program;
		else if (value == 0x90)
			m_command = command::bypass_reset;
		break;
	case command::bypass_reset:
		m_bypass = value != 0;
		m_command = m_bypass ? command::bypass : command::array;
		break;
	default:
		break;
	}
}
