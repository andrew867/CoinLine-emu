// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"
#include "diserial.h"

#include "millennium_smartcard_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_SMARTCARD, millennium_smartcard_device)

class millennium_smartcard_device : public device_t, public device_nvram_interface, private device_serial_interface {
public:
	millennium_smartcard_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	bool reload_fixture_from_path(std::string const &path, std::string &error_out, bool additional_slot = false);
	void insert_card(u64 cycle, u64 cpu_hz);
	void remove_card();
	std::uint8_t read_fifo(u64 cycle);
	void write_command(u8 data, u64 cycle);
	std::uint8_t status_lines() const;

	void notify_reset(u64 cycle);
	void later_pins(u8 d, u8 e, u8 f, u64 cycle);
	void later_async_pins(u8 h, u8 d, u8 e, millennium_smartcard_model *internal_contact = nullptr);
	bool async_routed() const noexcept { return m_serial_model != nullptr; }
	void rxd_w(int state);
	auto txd_handler() { return m_txd.bind(); }
	void earlier_pins(u8 a, u8 c, u64 cycle);
	u8 merge_data_pin(u8 value, u8 mask, u64 cycle) const;

	millennium_smartcard_model &model() noexcept { return m_model; }

private:
	void nvram_default() override;
	bool nvram_read(util::read_stream &file) override;
	bool nvram_write(util::write_stream &file) override;
	bool nvram_can_write() const override { return m_primary_writable_digest.size() == 64U; }
	void stop_async_serial();
	void schedule_async_serial();
	TIMER_CALLBACK_MEMBER(async_serial_tick);
	void tra_callback() override;
	void tra_complete() override;
	void rcv_complete() override;
	u64 async_cycle() const;
	static u8 inverse_octet(u8 value);
	devcb_write_line m_txd;
	emu_timer *m_async_timer = nullptr;
	attotime m_async_guard_until = attotime::zero;
	attotime m_async_period = attotime::never;
	bool m_async_serial = false;
	u64 m_async_hz = 12288000;
	millennium_smartcard_model m_model{};
	millennium_smartcard_model *m_serial_model = nullptr;
	bool m_pin_trace = false;
	bool m_lab_serial_trace = false;
	std::string m_fixture_path;
	std::vector<std::uint8_t> m_fixture_bytes;
	std::string m_fixture_digest;
	// The explicitly writable primary lab fixture retains its memory while
	// a read-only secondary contact fixture is selected.
	std::string m_primary_writable_digest;
	std::vector<std::uint8_t> m_primary_memory;
};
