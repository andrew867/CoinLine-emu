// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_flash.h"

#include <algorithm>
#include <cstdio>
#include <vector>

DEFINE_DEVICE_TYPE(MILLENNIUM_FLASH, millennium_flash_device, "mill_flash", "Millennium dual NOR flash")
ALLOW_SAVE_TYPE(millennium_flash_chip::command);
ALLOW_SAVE_TYPE(millennium_flash_chip::operation);

millennium_flash_device::millennium_flash_device(machine_config const &config,
	char const *tag, device_t *owner, std::uint32_t clock)
	: device_t(config, MILLENNIUM_FLASH, tag, owner, clock)
	, device_nvram_interface(config, *this)
{
}

void millennium_flash_device::device_start()
{
	for (unsigned index = 0; index < m_chips.size(); ++index) {
		auto &chip = m_chips[index];
		save_item(NAME(chip.bytes), index);
		save_item(NAME(chip.m_command), index);
		save_item(NAME(chip.m_operation), index);
		save_item(NAME(chip.m_until), index);
		save_item(NAME(chip.m_erase_start), index);
		save_item(NAME(chip.m_program_address), index);
		save_item(NAME(chip.m_program_value), index);
		save_item(NAME(chip.m_erase_mask), index);
		save_item(NAME(chip.m_toggle), index);
		save_item(NAME(chip.m_bypass), index);
		save_item(NAME(chip.m_program_error), index);
	}
}

std::uint64_t millennium_flash_device::now_us() const
{
	return machine().time().as_ticks(1000000);
}

void millennium_flash_device::seed(std::uint8_t const *data, std::string const &sha256, bool enabled)
{
	m_enabled = enabled;
	m_seed_sha256 = sha256;
	for (auto &chip : m_chips) {
		std::copy_n(data, chip.size, chip.bytes.begin());
		data += chip.size;
		chip.power_on();
	}
}

std::uint8_t millennium_flash_device::read(std::uint32_t address)
{
	if (address >= 2 * millennium_flash_chip::size)
		return 0xff;
	auto &chip = m_chips[address / millennium_flash_chip::size];
	if (chip.m_operation == millennium_flash_chip::operation::idle
		&& chip.m_command != millennium_flash_chip::command::autoselect
		&& chip.m_command != millennium_flash_chip::command::error)
		return raw(address);
	return machine().side_effects_disabled() ? raw(address) : chip.read(address, now_us());
}

void millennium_flash_device::write(std::uint32_t address, std::uint8_t value)
{
	if (m_enabled && address < 2 * millennium_flash_chip::size)
		m_chips[address / millennium_flash_chip::size].write(address, value, now_us());
}

std::uint8_t millennium_flash_device::raw(std::uint32_t address) const
{
	if (address >= 2 * millennium_flash_chip::size)
		return 0xff;
	return m_chips[address / millennium_flash_chip::size].bytes[address % millennium_flash_chip::size];
}

void millennium_flash_device::nvram_default()
{
	// The operator-supplied initial image was seeded before MAME loads NVRAM.
	if (m_enabled)
		std::fprintf(stderr, "program flash seed\n");
}

bool millennium_flash_device::nvram_read(util::read_stream &file)
{
	if (!m_enabled)
		return false;
	std::array<char, 64> digest{};
	auto const [header_error, header_size] = util::read(file, digest.data(), digest.size());
	if (header_error || header_size != digest.size()
		|| std::string(digest.data(), digest.size()) != m_seed_sha256)
		return false;
	std::vector<std::uint8_t> data(2 * millennium_flash_chip::size);
	auto const [error, actual] = util::read(file, data.data(), data.size());
	if (error || actual != data.size())
		return false;
	seed(data.data(), m_seed_sha256, true);
	std::fprintf(stderr, "program flash load\n");
	return true;
}

bool millennium_flash_device::nvram_write(util::write_stream &file)
{
	if (!m_enabled || m_seed_sha256.size() != 64)
		return false;
	auto const [header_error, header_size] = util::write(file, m_seed_sha256.data(), m_seed_sha256.size());
	if (header_error || header_size != m_seed_sha256.size())
		return false;
	for (auto &chip : m_chips) {
		chip.complete(now_us());
		auto const [error, actual] = util::write(file, chip.bytes.data(), chip.bytes.size());
		if (error || actual != chip.bytes.size())
			return false;
	}
	return true;
}
