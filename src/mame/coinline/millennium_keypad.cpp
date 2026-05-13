// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_keypad.h"

#include "millennium_audio_route.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_KEYPAD, millennium_keypad_device, "millennium_keypad", "CoinLine Millennium keypad")

millennium_keypad_device::millennium_keypad_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_KEYPAD, tag, owner, clock)
	, m_audroute(*this, "^:audroute")
{
}

void millennium_keypad_device::device_start()
{
}

void millennium_keypad_device::device_reset()
{
	device_t::device_reset();
	m_model.reset();
}

void millennium_keypad_device::apply_config(millennium_keypad_board_config const &cfg)
{
	m_model.configure(cfg);
	m_model.reset();
}

std::uint32_t millennium_keypad_device::keymatrix_with_hook_debounce(std::uint32_t raw_keymatrix,
	std::uint64_t cpu_cycle)
{
	return m_model.keymatrix_with_hook_debounce(raw_keymatrix, cpu_cycle);
}

void millennium_keypad_device::poll_handset_hook()
{
	// TP owns hook supervision and reports transitions over the telephony link.
	// Keep keypad device from directly steering audio hook state.
}

u8 millennium_keypad_device::read(std::uint64_t cpu_cycle, offs_t offset)
{
	poll_handset_hook();
	// Front-panel keys, hook, and side keys are not wired to the CP 8255 — they are read by
	// the telephony processor and reach the CP as CSI/O opcodes. PIO port A/C reads therefore
	// see an idle matrix / discretes; only IP-comm RTS (PA bit 6) is synthesized here.
	static constexpr std::uint32_t k_no_front_panel_on_pio = 0U;
	switch (offset & 3) {
	case 0: {
		// Port A is not a pure keypad-row nibble: firmware also samples IP-comm RTS on bit 6 (active low).
		u8 pa = u8(m_model.read_port_a(k_no_front_panel_on_pio, cpu_cycle) & 0x0f);
		pa |= 0xf0;
		if (m_ip_comm_rts_asserted)
			pa &= ~0x40U;
		return pa;
	}
	case 1: return m_model.read_port_b(k_no_front_panel_on_pio, cpu_cycle);
	case 2: return m_model.read_port_c(k_no_front_panel_on_pio, cpu_cycle);
	default: return 0xff;
	}
}

void millennium_keypad_device::write(std::uint64_t cpu_cycle, offs_t offset, u8 data)
{
	poll_handset_hook();
	switch (offset & 3) {
	case 1: m_model.write_port_b(data, cpu_cycle); break;
	case 2: m_model.write_port_c(data, cpu_cycle); break;
	case 3: m_model.write_control(data, cpu_cycle); break;
	default: break;
	}
	(void)cpu_cycle;
}
