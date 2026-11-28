// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_flash.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
	m_program_begin_count = 0;
	m_program_complete_count = 0;
	m_erase_begin_count = 0;
	m_erase_complete_count = 0;
	if (char const *path = std::getenv("COINLINE_FLASH_TRACE"); path && *path) {
		m_trace.open(path, std::ios::out | std::ios::trunc);
		if (!m_trace)
			osd_printf_warning("millennium: cannot open flash trace\n");
	}
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

void millennium_flash_device::device_stop()
{
	if (!m_trace)
		return;
	m_trace << "{\"event\":\"summary\",\"program_begin\":" << m_program_begin_count
		<< ",\"program_complete\":" << m_program_complete_count
		<< ",\"erase_begin\":" << m_erase_begin_count
		<< ",\"erase_complete\":" << m_erase_complete_count << "}\n";
	m_trace.close();
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
	unsigned const chip_index = address / millennium_flash_chip::size;
	auto &chip = m_chips[chip_index];
	if (chip.m_operation == millennium_flash_chip::operation::idle
		&& chip.m_command != millennium_flash_chip::command::autoselect
		&& chip.m_command != millennium_flash_chip::command::error)
		return raw(address);
	if (machine().side_effects_disabled())
		return raw(address);
	auto const before_operation = chip.m_operation;
	auto const before_address = chip.m_program_address;
	auto const before_value = chip.m_program_value;
	auto const before_mask = chip.m_erase_mask;
	auto const before_error = chip.m_program_error;
	auto const now = now_us();
	auto const value = chip.read(address, now);
	if (before_operation != millennium_flash_chip::operation::idle
		&& chip.m_operation == millennium_flash_chip::operation::idle) {
		bool const programmed = before_operation == millennium_flash_chip::operation::program;
		if (programmed)
			++m_program_complete_count;
		else
			++m_erase_complete_count;
		trace_event(programmed ? "program_complete" : "erase_complete", chip_index,
			programmed ? before_address : address % millennium_flash_chip::size,
			before_value, programmed ? chip.bytes[before_address] : 0xff,
			before_mask, before_error, now);
	}
	return value;
}

void millennium_flash_device::write(std::uint32_t address, std::uint8_t value)
{
	if (!m_enabled || address >= 2 * millennium_flash_chip::size)
		return;
	unsigned const chip_index = address / millennium_flash_chip::size;
	auto &chip = m_chips[chip_index];
	auto const local_address = address % millennium_flash_chip::size;
	auto const before_operation = chip.m_operation;
	auto const before_until = chip.m_until;
	auto const before_address = chip.m_program_address;
	auto const before_value = chip.m_program_value;
	auto const before_mask = chip.m_erase_mask;
	auto const before_error = chip.m_program_error;
	auto const previous = chip.bytes[local_address];
	auto const now = now_us();
	chip.write(local_address, value, now);
	bool const previous_completed = before_operation != millennium_flash_chip::operation::idle
		&& now >= before_until;
	if (previous_completed) {
		bool const programmed = before_operation == millennium_flash_chip::operation::program;
		if (programmed)
			++m_program_complete_count;
		else
			++m_erase_complete_count;
		trace_event(programmed ? "program_complete" : "erase_complete", chip_index,
			programmed ? before_address : local_address, before_value,
			programmed ? chip.bytes[before_address] : 0xff, before_mask,
			before_error, now);
	}
	bool const started = chip.m_operation != millennium_flash_chip::operation::idle
		&& (before_operation == millennium_flash_chip::operation::idle || previous_completed);
	if (started) {
		bool const programmed = chip.m_operation == millennium_flash_chip::operation::program;
		if (programmed)
			++m_program_begin_count;
		else
			++m_erase_begin_count;
		trace_event(programmed ? "program_begin" : "erase_begin", chip_index,
			programmed ? chip.m_program_address : local_address,
			programmed ? chip.m_program_value : value, previous,
			chip.m_erase_mask, chip.m_program_error, now);
	} else if (before_operation == millennium_flash_chip::operation::erase
		&& chip.m_operation == millennium_flash_chip::operation::erase
		&& chip.m_erase_mask != before_mask) {
		trace_event("erase_select", chip_index, local_address, value, previous,
			chip.m_erase_mask, false, now);
	}
}

void millennium_flash_device::trace_event(char const *event, unsigned chip,
	std::uint32_t address, std::uint8_t value, std::uint8_t previous,
	std::uint8_t erase_mask, bool program_error, std::uint64_t emulated_us)
{
	if (!m_trace)
		return;
	std::uint32_t const mapped = std::uint32_t(chip * millennium_flash_chip::size) + address;
	m_trace << "{\"event\":\"" << event << "\",\"emulated_us\":" << emulated_us
		<< ",\"chip\":" << chip << ",\"address\":" << mapped
		<< ",\"block_4k\":" << mapped / 4096
		<< ",\"sector_64k\":" << mapped / millennium_flash_chip::block_size
		<< ",\"value\":" << unsigned(value) << ",\"previous\":" << unsigned(previous)
		<< ",\"erase_mask\":" << unsigned(erase_mask)
		<< ",\"program_error\":" << (program_error ? "true" : "false") << "}\n";
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
	for (unsigned index = 0; index < m_chips.size(); ++index) {
		auto &chip = m_chips[index];
		auto const before_operation = chip.m_operation;
		auto const before_address = chip.m_program_address;
		auto const before_value = chip.m_program_value;
		auto const before_mask = chip.m_erase_mask;
		auto const before_error = chip.m_program_error;
		auto const now = now_us();
		chip.complete(now);
		if (before_operation != millennium_flash_chip::operation::idle
			&& chip.m_operation == millennium_flash_chip::operation::idle) {
			bool const programmed = before_operation == millennium_flash_chip::operation::program;
			if (programmed)
				++m_program_complete_count;
			else
				++m_erase_complete_count;
			trace_event(programmed ? "program_complete" : "erase_complete", index,
				programmed ? before_address : 0, before_value,
				programmed ? chip.bytes[before_address] : 0xff, before_mask,
				before_error, now);
		}
		auto const [error, actual] = util::write(file, chip.bytes.data(), chip.bytes.size());
		if (error || actual != chip.bytes.size())
			return false;
	}
	return true;
}
