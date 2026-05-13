// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_coin.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_COIN, millennium_coin_device, "millennium_coin", "CoinLine Millennium coin validator")

millennium_coin_device::millennium_coin_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_COIN, tag, owner, clock)
{
}

void millennium_coin_device::device_start()
{
}

void millennium_coin_device::device_reset()
{
	device_t::device_reset();
	m_model.reset();
}

void millennium_coin_device::apply_config(millennium_coin_board_config const &cfg)
{
	m_model.configure(cfg);
	m_model.reset();
}

bool millennium_coin_device::begin_insert_cents(int cents, std::uint64_t cpu_cycle, std::uint64_t cpu_hz)
{
	return m_model.begin_insert_cents(cents, cpu_cycle, cpu_hz);
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
