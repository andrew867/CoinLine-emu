// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_coin.h"
#include <algorithm>
#include <array>

DEFINE_DEVICE_TYPE(MILLENNIUM_COIN, millennium_coin_device, "millennium_coin", "CoinLine Millennium coin validator")

millennium_coin_device::millennium_coin_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_COIN, tag, owner, clock)
	, device_nvram_interface(mconfig, *this)
	, device_buffered_serial_interface(mconfig, *this)
	, m_txd(*this)
{
}

void millennium_coin_device::device_start()
{
}

void millennium_coin_device::device_reset()
{
	device_t::device_reset();
	m_model.reset();
	set_data_frame(1, 8, PARITY_NONE, STOP_BITS_1);
	set_rate(600);
	receive_register_reset();
	transmit_register_reset();
	clear_fifo();
	m_txd(1);
}

void millennium_coin_device::apply_config(millennium_coin_board_config const &cfg)
{
	m_model.configure(cfg);
	m_model.reset();
}

bool millennium_coin_device::begin_insert_cents(int cents, std::uint64_t cpu_cycle, std::uint64_t cpu_hz)
{
	if (m_model.uart_active()) {
		auto const code = m_model.uart_insert(cents, machine().time().as_ticks(1000));
		if (code)
			transmit_byte(*code);
		return code.has_value();
	}
	return m_model.begin_insert_cents(cents, cpu_cycle, cpu_hz);
}

void millennium_coin_device::received_byte(u8 data)
{
	for (u8 const response : m_model.uart_command(data, machine().time().as_ticks(1000)))
		transmit_byte(response);
}

void millennium_coin_device::tra_callback()
{
	m_txd(transmit_register_get_data_bit());
}

void millennium_coin_device::write_control(std::uint8_t data, std::uint64_t cpu_cycle)
{
	m_model.write_control(data, cpu_cycle);
}

std::uint8_t millennium_coin_device::read_status(std::uint64_t cpu_cycle)
{
	return m_model.read_status(cpu_cycle);
}

void millennium_coin_device::inject_jam(std::uint64_t cpu_cycle)
{
	m_model.inject_jam(cpu_cycle);
}

void millennium_coin_device::inject_reject_route(std::uint64_t cpu_cycle)
{
	m_model.inject_reject_route(cpu_cycle);
}

void millennium_coin_device::set_cashbox_state(bool present, bool full)
{
	m_model.advance_uart(machine().time().as_ticks(1000));
	m_model.set_cashbox_state(present, full);
}

namespace {
constexpr std::array<u8, 8> coin_header{'C','L','C','O','I','N','1',0};
}

void millennium_coin_device::nvram_default()
{
	m_model.restore_contents({});
}

bool millennium_coin_device::nvram_can_write() const
{
	auto const c = m_model.contents();
	return c.escrow || c.cashbox || c.refunded || c.removed;
}

bool millennium_coin_device::nvram_read(util::read_stream &file)
{
	std::array<u8, 40> bytes{};
	auto const [error, size] = util::read(file, bytes.data(), bytes.size());
	if (error || size != bytes.size() || !std::equal(coin_header.begin(), coin_header.end(), bytes.begin())) return false;
	std::array<u64, 4> counts{};
	for (unsigned index = 0; index < counts.size(); ++index)
		for (unsigned octet = 0; octet < 8; ++octet)
			counts[index] |= u64(bytes[8 + index * 8 + octet]) << (8 * octet);
	m_model.restore_contents({counts[0], counts[1], counts[2], counts[3]});
	return true;
}

bool millennium_coin_device::nvram_write(util::write_stream &file)
{
	m_model.advance_uart(machine().time().as_ticks(1000));
	auto const c = m_model.contents();
	std::array<u64, 4> counts{c.escrow, c.cashbox, c.refunded, c.removed};
	std::array<u8, 40> bytes{};
	std::copy(coin_header.begin(), coin_header.end(), bytes.begin());
	for (unsigned index = 0; index < counts.size(); ++index)
		for (unsigned octet = 0; octet < 8; ++octet)
			bytes[8 + index * 8 + octet] = u8(counts[index] >> (8 * octet));
	auto const [error, size] = util::write(file, bytes.data(), bytes.size());
	return !error && size == bytes.size();
}
