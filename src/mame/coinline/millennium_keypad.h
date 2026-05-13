// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

#include "millennium_keypad_model.h"

DECLARE_DEVICE_TYPE(MILLENNIUM_KEYPAD, millennium_keypad_device)

class millennium_audio_route_device;

class millennium_keypad_device : public device_t {
public:
	millennium_keypad_device(machine_config const &mconfig, char const *tag, device_t *owner, u32 clock = 0);

	void device_start() override ATTR_COLD;
	void device_reset() override;

	void apply_config(millennium_keypad_board_config const &cfg);

	/// Panel bitmap **for the TP behavioral model only** — hook debounce here feeds CSI/O (TP→CP), never CP 8255 reads.
	std::uint32_t keymatrix_with_hook_debounce(std::uint32_t raw_keymatrix, std::uint64_t cpu_cycle);

	u8 read(std::uint64_t cpu_cycle, offs_t offset);
	void write(std::uint64_t cpu_cycle, offs_t offset, u8 data);
	void set_ip_comm_rts_asserted(bool asserted) noexcept { m_ip_comm_rts_asserted = asserted; }

	bool consume_m7_pending() noexcept { return m_model.consume_m7_pending(); }
	std::uint64_t keypad_scan_count() const noexcept { return m_model.matrix_read_count(); }

private:
	/// Placeholder — hook/audio coupling is not driven from this device (handset is modeled on KEYMATRIX → TP CSI/O).
	void poll_handset_hook();

	optional_device<millennium_audio_route_device> m_audroute;
	millennium_keypad_model m_model{};
	bool m_handset_latched = false;
	bool m_ip_comm_rts_asserted = false;
};
