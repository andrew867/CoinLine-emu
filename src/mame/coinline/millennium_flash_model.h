// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// ST M29F040B: 512 KiB x8 NOR flash, eight uniform erase blocks.
class millennium_flash_chip {
public:
	static constexpr std::size_t size = 0x80000;
	static constexpr std::size_t block_size = 0x10000;

	std::uint8_t read(std::uint32_t address, std::uint64_t now_us);
	void write(std::uint32_t address, std::uint8_t value, std::uint64_t now_us);
	void complete(std::uint64_t now_us);
	void power_on();

	enum class command : std::uint8_t {
		array, unlock, select, erase_unlock, erase_select, erase_confirm,
		program, autoselect, bypass, bypass_program, bypass_reset, error
	};
	enum class operation : std::uint8_t { idle, program, erase };

	std::array<std::uint8_t, size> bytes{};

private:
	friend class millennium_flash_device;
	command m_command = command::array;
	operation m_operation = operation::idle;
	std::uint64_t m_until = 0;
	std::uint64_t m_erase_start = 0;
	std::uint32_t m_program_address = 0;
	std::uint8_t m_program_value = 0xff;
	std::uint8_t m_erase_mask = 0;
	std::uint8_t m_toggle = 0;
	bool m_bypass = false;
	bool m_program_error = false;
};
