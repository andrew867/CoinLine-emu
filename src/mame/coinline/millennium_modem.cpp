// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_modem.h"

DEFINE_DEVICE_TYPE(MILLENNIUM_MODEM, millennium_modem_device, "millennium_modem", "CoinLine Modem UART")

millennium_modem_device::millennium_modem_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_MODEM, tag, owner, clock)
	, device_serial_interface(mconfig, *this)
{
}

void millennium_modem_device::device_start()
{
}

void millennium_modem_device::device_reset()
{
	m_model.reset();
	m_terminal_tx_period = attotime::never;
	set_rcv_rate(attotime::never);
	receive_register_reset();
}

void millennium_modem_device::configure_terminal_tx(attotime period, u8 control_a, u8 control_b)
{
	u8 const frame = (control_a & 7U) | (control_b & 0x10U);
	if (period == m_terminal_tx_period && frame == m_terminal_tx_frame)
		return;
	m_terminal_tx_period = period;
	m_terminal_tx_frame = frame;
	parity_t const parity = (frame & 2U) == 0 ? PARITY_NONE : (frame & 0x10U) ? PARITY_ODD : PARITY_EVEN;
	set_data_frame(1, (frame & 4U) ? 8 : 7, parity, (frame & 1U) ? STOP_BITS_2 : STOP_BITS_1);
	set_rcv_rate(period);
	receive_register_reset();
}

void millennium_modem_device::rcv_complete()
{
	receive_register_extract();
	if (samples_terminal_tx() && !is_receive_framing_error() && !is_receive_parity_error())
		m_model.note_tx(get_received_char());
}
