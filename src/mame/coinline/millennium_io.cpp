// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_io.h"
#include "millennium_state.h"

void millennium_configure_io_map(address_map &map, millennium_state &state)
{
	// The graphical expansion decodes the full address. The driver retains
	// low-byte mirroring for all conventional peripherals.
	map(0x0000, 0xffff).r(state, FUNC(millennium_state::external_io_r))
		.w(state, FUNC(millennium_state::external_io_w));
}

u8 millennium_state::external_io_r(offs_t offset)
{
	u16 const port = u16(offset);
	u8 const low = u8(port);
	if (m_graphic_display) {
		if (millennium_graphic_vfd::pixel_address(port)) {
			trace_graphic_vfd(port, 'r', m_unknown_default);
			return m_unknown_default; // No pixel readback interface.
		}
		if (low == 0x80) {
			// Independent, active-low analogue expansion outputs.
			u8 status = catch_all_io_r(low) | 0x0cU;
			if (m_adsi_rx.cas_detected()) status &= ~0x04U;
			if (m_adsi_rx.fsk_carrier()) status &= ~0x08U;
			u8 const result = m_graphic_vfd.read_status(status, m_maincpu->total_cycles());
			trace_graphic_vfd(port, 'r', result);
			return result;
		}
		if (low == 0x90) {
			u16 const pressed = u16(m_terminal21_softkeys_io->read() & 0x0fffU);
			u8 const result = m_graphic_vfd.read_keys(m_unknown_default, pressed);
			trace_graphic_vfd(port, 'r', result, pressed);
			return result;
		}
		if (low == 0x60)
			return m_unknown_default;
	}
	switch (low) {
	case 0x40: return board_status_r();
	case 0x41: case 0x42: case 0x43: case 0x44: return pio_keypad_r(low - 0x41);
	case 0x52: return card_status_r();
	case 0x53: return card_data_r();
	case 0x54: return coin_status_r();
	case 0x60: return vfd_status_r();
	case 0x61: return voiceware_phrase_r();
	case 0x63: return pio_port_g_r();
	case 0xc0: case 0xc1: case 0xc2: case 0xc3: return mach_pio_r(low - 0xc0);
	case 0xe0: case 0xe1: case 0xe2: case 0xe3:
	case 0xe4: case 0xe5: case 0xe6: case 0xe7: return external_uart_r(low - 0xe0);
	default: return catch_all_io_r(low);
	}
}

void millennium_state::external_io_w(offs_t offset, u8 data)
{
	u16 const port = u16(offset);
	u8 const low = u8(port);
	if (m_graphic_display) {
		if (millennium_graphic_vfd::pixel_address(port)) {
			m_graphic_vfd.write_pixel(port, data, m_maincpu->total_cycles());
			trace_graphic_vfd(port, 'w', data);
			return;
		}
		switch (low) {
		case 0x80:
			m_graphic_vfd.write_control(data, m_maincpu->total_cycles());
			trace_graphic_vfd(port, 'w', data);
			return;
		case 0x90:
			m_graphic_vfd.write_keys(data);
			trace_graphic_vfd(port, 'w', data);
			return;
		case 0xf0:
			if (data == 0)
				m_graphic_vfd.release_reset();
			trace_graphic_vfd(port, 'w', data);
			return;
		case 0x60:
			return; // Not a character VFD on this profile.
		}
	}
	switch (low) {
	case 0x40: board_status_w(data); break;
	case 0x41: case 0x42: case 0x43: case 0x44: pio_keypad_w(low - 0x41, data); break;
	case 0x52: card_status_w(data); break;
	case 0x53: card_data_w(data); break;
	case 0x55: coin_control_w(data); break;
	case 0x58: audio_tone_w(data); break;
	case 0x59: audio_dtmf_ascii_w(data); break;
	case 0x5a: audio_dtmf_duration_w(data); break;
	case 0x5b: audio_vol_w(data); break;
	case 0x60: vfd_display_w(data); break;
	case 0x61: voiceware_phrase_w(data); break;
	case 0x63: pio_port_g_w(data); break;
	case 0xc0: case 0xc1: case 0xc2: case 0xc3: mach_pio_w(low - 0xc0, data); break;
	case 0xe0: case 0xe1: case 0xe2: case 0xe3:
	case 0xe4: case 0xe5: case 0xe6: case 0xe7: external_uart_w(low - 0xe0, data); break;
	default: catch_all_io_w(low, data); break;
	}
}
