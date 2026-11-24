// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"
#include "diserial.h"

#include "millennium_coin_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_COIN, millennium_coin_device)

class millennium_coin_device : public device_t, private device_buffered_serial_interface<16> {
public:
	millennium_coin_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void apply_config(millennium_coin_board_config const &cfg);

	bool begin_insert_cents(int cents, std::uint64_t cpu_cycle, std::uint64_t cpu_hz);
	void write_control(std::uint8_t data, std::uint64_t cpu_cycle);
	std::uint8_t read_status(std::uint64_t cpu_cycle);

	void inject_jam(std::uint64_t cpu_cycle);
	void inject_reject_route(std::uint64_t cpu_cycle);

	void set_cpu_hz(std::uint64_t hz) noexcept { m_model.set_cpu_hz(hz); }
	void rxd_w(int state) { rx_w(state); }
	auto txd_handler() { return m_txd.bind(); }

	millennium_coin_model &model() noexcept { return m_model; }

private:
	void received_byte(u8 data) override;
	void tra_callback() override;
	devcb_write_line m_txd;
	millennium_coin_model m_model{};
};
