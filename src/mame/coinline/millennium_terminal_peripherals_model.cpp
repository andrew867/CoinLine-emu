// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_terminal_peripherals_model.h"

namespace coinline::terminal {

void ring_alerter_model::disable(std::uint8_t reason) noexcept
{
	m_disable_mask |= reason;
}

void ring_alerter_model::enable(std::uint8_t reason) noexcept
{
	m_disable_mask = static_cast<std::uint8_t>(m_disable_mask & ~reason);
}

void ring_alerter_model::on_ring_on() noexcept
{
	m_is_ringing = true;
	m_ring_pulse_count++;
	if (m_disable_mask == 0U)
		m_incoming_emitted = true;
}

void ring_alerter_model::on_ring_abandoned() noexcept
{
	if (m_is_ringing && m_disable_mask == 0U)
		m_abandoned_emitted = true;
	m_is_ringing = false;
}

void ring_alerter_model::on_ring_timeout() noexcept
{
	if (m_is_ringing && m_disable_mask == 0U)
		m_abandoned_emitted = true;
	m_is_ringing = false;
}

void ring_alerter_model::clear_events() noexcept
{
	m_incoming_emitted = false;
	m_abandoned_emitted = false;
	m_ring_pulse_count = 0U;
}

void forgotten_card_alarm_model::start(
	unsigned on_delay_ticks, unsigned cadence_on_ticks, unsigned cadence_off_ticks, unsigned total_ticks) noexcept
{
	m_on_delay = on_delay_ticks;
	m_cadence_on = cadence_on_ticks;
	m_cadence_off = cadence_off_ticks;
	m_total = total_ticks;
	m_t = 0U;
	m_cadence_t = 0U;
	m_armed = true;
	m_active = false;
	m_tone_on = false;
	m_relay_warning = false;
}

void forgotten_card_alarm_model::cancel() noexcept
{
	m_armed = false;
	m_active = false;
	m_tone_on = false;
	m_relay_warning = false;
}

void forgotten_card_alarm_model::tick() noexcept
{
	if (!m_armed && !m_active)
		return;

	if (m_armed) {
		m_t++;
		if (m_t >= m_on_delay) {
			m_armed = false;
			m_active = true;
			m_tone_on = true;
			m_relay_warning = true;
			m_t = 0U;
			m_cadence_t = 0U;
		}
		return;
	}

	m_t++;
	if (m_total != 0U && m_t >= m_total) {
		cancel();
		return;
	}

	m_cadence_t++;
	if (m_tone_on) {
		if (m_cadence_t >= m_cadence_on) {
			m_tone_on = false;
			m_cadence_t = 0U;
		}
	} else if (m_cadence_t >= m_cadence_off) {
		m_tone_on = true;
		m_cadence_t = 0U;
	}
}

void forgotten_card_alarm_model::acknowledge_user_return() noexcept
{
	cancel();
}

void data_jack_model::manual_keypad_digit_signal() noexcept
{
	if (m_state != relay_idle)
		return;
	m_control_on = true;
	m_call_established = false;
	m_data_session_active = false;
	m_disconnect_supervision_pending = false;
	m_state = relay_connecting_modem_sink;
}

void data_jack_model::on_relay_sequence_complete_event() noexcept
{
	if (m_state == relay_connecting_modem_sink) {
		m_state = relay_dialing_completed;
		return;
	}
	if (m_state == relay_disconnecting_modem_sink) {
		m_state = relay_idle;
		m_control_on = false;
		m_call_established = true;
		m_data_session_active = false;
		m_disconnect_supervision_pending = false;
	}
}

void data_jack_model::on_dialing_complete_event() noexcept
{
	if (m_state == relay_dialing_completed)
		m_state = relay_connected_data_active;
}

void data_jack_model::on_laptop_drop() noexcept
{
	m_state = relay_idle;
	m_control_on = false;
	m_call_established = false;
	m_data_session_active = false;
	m_disconnect_supervision_pending = false;
}

void data_jack_model::on_data_session_established() noexcept
{
	if (m_state == relay_connected_data_active)
		m_data_session_active = true;
}

void data_jack_model::on_disconnect_delay_expired() noexcept
{
	if (m_state == relay_connected_data_active) {
		m_state = relay_disconnect_delay;
		m_disconnect_supervision_pending = true;
		return;
	}
	if (m_state == relay_disconnect_delay)
		m_state = relay_disconnecting_modem_sink;
}

bool eeprom_model::read(std::size_t offset, std::size_t size, std::vector<std::uint8_t> &out) const noexcept
{
	out.clear();
	if (offset > m_bytes.size() || size > (m_bytes.size() - offset))
		return false;
	out.insert(out.end(), m_bytes.begin() + static_cast<std::ptrdiff_t>(offset),
		m_bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
	return true;
}

bool eeprom_model::write(std::size_t offset, std::vector<std::uint8_t> const &data) noexcept
{
	if (offset > m_bytes.size() || data.size() > (m_bytes.size() - offset))
		return false;
	for (std::size_t i = 0; i < data.size(); ++i)
		m_bytes[offset + i] = data[i];
	return true;
}

bool eeprom_model::verify(std::size_t offset, std::vector<std::uint8_t> const &data) const noexcept
{
	if (offset > m_bytes.size() || data.size() > (m_bytes.size() - offset))
		return false;
	for (std::size_t i = 0; i < data.size(); ++i) {
		if (m_bytes[offset + i] != data[i])
			return false;
	}
	return true;
}

bool eeprom_model::write_record_with_checksum(std::size_t offset, std::vector<std::uint8_t> const &payload) noexcept
{
	std::uint8_t checksum = 0U;
	for (std::uint8_t b : payload)
		checksum = static_cast<std::uint8_t>(checksum ^ b);
	std::vector<std::uint8_t> framed = payload;
	framed.push_back(checksum);
	return write(offset, framed);
}

bool eeprom_model::read_record_with_checksum(std::size_t offset, std::size_t payload_size,
	std::vector<std::uint8_t> &payload_out) const noexcept
{
	std::vector<std::uint8_t> framed;
	if (!read(offset, payload_size + 1U, framed))
		return false;
	std::uint8_t checksum = 0U;
	for (std::size_t i = 0; i < payload_size; ++i)
		checksum = static_cast<std::uint8_t>(checksum ^ framed[i]);
	if (checksum != framed[payload_size])
		return false;
	payload_out.assign(framed.begin(), framed.begin() + static_cast<std::ptrdiff_t>(payload_size));
	return true;
}

void cashbox_collection_model::commit_record(unsigned box_id, bool upload_required) noexcept
{
	m_status = cbxst_new_status;
	if (!m_box_set || m_box != box_id) {
		m_status = static_cast<std::uint8_t>(m_status | cbxst_new_box);
		m_seq = 0U;
		m_box = box_id;
		m_box_set = true;
	}
	if (upload_required)
		m_status = static_cast<std::uint8_t>(m_status | cbxst_coll_dlog_req);
	m_seq++;
	m_last_payload.box_id = m_box;
	m_last_payload.sequence = m_seq;
	m_last_payload.status = m_status;
}

void peripheral_health_model::on_telephony_alive_result(bool ok) noexcept
{
	if (ok) {
		m_tel_miss = 0U;
		m_alarm_tel_not_responding = false;
		return;
	}
	m_tel_miss++;
	if (m_tel_miss >= 3U)
		m_alarm_tel_not_responding = true;
}

void peripheral_health_model::on_coin_alive_result(bool ok) noexcept
{
	m_alarm_coin_hw_fault = !ok;
}

void peripheral_health_model::on_card_sensor_idle(bool idle) noexcept
{
	if (idle) {
		m_card_blocked = 0U;
		m_alarm_card_reader_blocked = false;
		return;
	}
	m_card_blocked++;
	if (m_card_blocked >= 3U)
		m_alarm_card_reader_blocked = true;
}

void peripheral_health_model::on_coin_fault_sample(bool fault) noexcept
{
	if (fault) {
		m_coin_fault_consecutive++;
		m_coin_recover_consecutive = 0U;
		if (m_coin_fault_consecutive >= 2U)
			m_alarm_coin_hw_fault = true;
		return;
	}
	m_coin_fault_consecutive = 0U;
	m_coin_recover_consecutive++;
	if (m_coin_recover_consecutive >= 2U)
		m_alarm_coin_hw_fault = false;
}

void peripheral_health_model::on_card_blocked_sample(bool blocked) noexcept
{
	if (blocked) {
		m_card_blocked++;
		m_card_recover_consecutive = 0U;
		if (m_card_blocked >= 3U)
			m_alarm_card_reader_blocked = true;
		return;
	}
	m_card_recover_consecutive++;
	if (m_card_recover_consecutive >= 2U) {
		m_card_blocked = 0U;
		m_alarm_card_reader_blocked = false;
	}
}

void peripheral_health_model::request_owner(shared_owner owner) noexcept
{
	if (m_owner == owner_none)
		m_owner = owner;
}

void peripheral_health_model::force_disconnect_supervision_priority() noexcept
{
	m_owner = owner_disconnect_supervision;
}

void peripheral_health_model::release_owner(shared_owner owner) noexcept
{
	if (m_owner == owner)
		m_owner = owner_none;
}

} // namespace coinline::terminal
