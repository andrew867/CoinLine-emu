// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_terminal_peripherals_model.h"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace coinline::terminal;

int main()
{
	{
		ring_alerter_model m;
		m.on_ring_on();
		m.on_ring_abandoned();
		if (!m.incoming_ring_emitted() || !m.ring_abandoned_emitted()) {
			std::cerr << "ring nominal path failed\n";
			return 1;
		}
		m.clear_events();
		m.disable(ring_alerter_model::reason_voice);
		m.on_ring_on();
		if (m.incoming_ring_emitted() || m.enring_asserted()) {
			std::cerr << "voice disable did not block ring output\n";
			return 2;
		}
		m.enable(ring_alerter_model::reason_voice);
		if (!m.enring_asserted()) {
			std::cerr << "ring enable did not restore ENRING\n";
			return 3;
		}
		m.on_ring_on();
		m.on_ring_timeout();
		if (m.ring_pulse_count() < 1U || !m.ring_abandoned_emitted()) {
			std::cerr << "terminal_10 ring timeout vector failed\n";
			return 32;
		}
	}

	{
		forgotten_card_alarm_model m;
		m.start(2U, 3U, 4U, 12U);
		m.tick();
		if (!m.active() || m.tone_on()) {
			std::cerr << "alarm armed phase invalid\n";
			return 4;
		}
		m.tick();
		if (!m.tone_on() || !m.relay_warning_path()) {
			std::cerr << "alarm did not start after delay\n";
			return 5;
		}
		for (unsigned i = 0; i < 3U; ++i)
			m.tick();
		if (m.tone_on()) {
			std::cerr << "cadence off transition missing\n";
			return 6;
		}
		for (unsigned i = 0; i < 20U; ++i)
			m.tick();
		if (m.active() || m.tone_on() || m.relay_warning_path()) {
			std::cerr << "alarm duration cleanup failed\n";
			return 7;
		}
		m.start(1U, 1U, 1U, 10U);
		m.tick();
		m.acknowledge_user_return();
		if (m.active()) {
			std::cerr << "terminal_11 user-return cancel path failed\n";
			return 33;
		}
	}

	{
		data_jack_model m;
		m.manual_keypad_digit_signal();
		if (m.state() != data_jack_model::relay_connecting_modem_sink || !m.control_on()) {
			std::cerr << "data jack did not enter relay connect state\n";
			return 8;
		}
		m.on_relay_sequence_complete_event();
		m.on_dialing_complete_event();
		if (m.state() != data_jack_model::relay_connected_data_active) {
			std::cerr << "data jack did not enter active data state\n";
			return 9;
		}
		m.on_data_session_established();
		if (!m.data_session_active()) {
			std::cerr << "data jack active session flag not set\n";
			return 29;
		}
		m.on_disconnect_delay_expired();
		if (m.state() != data_jack_model::relay_disconnect_delay || !m.disconnect_supervision_pending()) {
			std::cerr << "data jack disconnect delay state missing\n";
			return 30;
		}
		m.on_disconnect_delay_expired();
		if (m.state() != data_jack_model::relay_disconnecting_modem_sink) {
			std::cerr << "data jack disconnect transition missing\n";
			return 31;
		}
		m.on_relay_sequence_complete_event();
		if (m.state() != data_jack_model::relay_idle || m.control_on() || !m.call_established()) {
			std::cerr << "data jack spill did not return to idle\n";
			return 10;
		}
		m.manual_keypad_digit_signal();
		m.on_laptop_drop();
		if (m.state() != data_jack_model::relay_idle || m.control_on()) {
			std::cerr << "laptop drop cleanup failed\n";
			return 11;
		}
	}

	{
		eeprom_model m(32U);
		std::vector<std::uint8_t> write_data{0x5AU, 0x11U};
		if (!m.write(0x10U, write_data) || !m.verify(0x10U, write_data)) {
			std::cerr << "eeprom write/verify nominal failed\n";
			return 12;
		}
		std::vector<std::uint8_t> read_data;
		if (!m.read(0x10U, 2U, read_data) || read_data != write_data) {
			std::cerr << "eeprom readback mismatch\n";
			return 13;
		}
		if (m.read(0xFFU, 1U, read_data) || m.write(31U, write_data) || m.verify(31U, write_data)) {
			std::cerr << "eeprom bounds guard failed\n";
			return 14;
		}
		std::vector<std::uint8_t> const payload{0xAAU, 0x55U, 0x0FU};
		std::vector<std::uint8_t> decoded;
		if (!m.write_record_with_checksum(0U, payload) || !m.read_record_with_checksum(0U, payload.size(), decoded)
			|| decoded != payload) {
			std::cerr << "terminal_12 checksum payload path failed\n";
			return 34;
		}
	}

	{
		cashbox_collection_model m;
		m.commit_record(100U, true);
		std::uint8_t const s1 = m.status();
		if ((s1 & cashbox_collection_model::cbxst_new_box) == 0U || (s1 & cashbox_collection_model::cbxst_new_status) == 0U
			|| (s1 & cashbox_collection_model::cbxst_coll_dlog_req) == 0U || m.sequence() != 1U) {
			std::cerr << "cashbox first record flags incorrect\n";
			return 15;
		}
		m.commit_record(100U, false);
		if (m.sequence() != 2U || (m.status() & cashbox_collection_model::cbxst_new_box) != 0U) {
			std::cerr << "cashbox same-box sequence or flags incorrect\n";
			return 16;
		}
		m.commit_record(101U, false);
		if (m.sequence() != 1U || m.current_box() != 101U
			|| (m.status() & cashbox_collection_model::cbxst_new_box) == 0U) {
			std::cerr << "cashbox new-box reset path failed\n";
			return 17;
		}
		auto const p = m.last_payload();
		if (p.box_id != 101U || p.sequence != 1U || (p.status & cashbox_collection_model::cbxst_new_box) == 0U) {
			std::cerr << "terminal_13 record payload semantics failed\n";
			return 35;
		}
	}

	{
		peripheral_health_model m;
		m.on_telephony_alive_result(false);
		m.on_telephony_alive_result(false);
		if (m.alarm_tel_not_responding()) {
			std::cerr << "telephony alarm raised too early\n";
			return 18;
		}
		m.on_telephony_alive_result(false);
		if (!m.alarm_tel_not_responding()) {
			std::cerr << "telephony alarm not raised at threshold\n";
			return 19;
		}
		m.on_telephony_alive_result(true);
		if (m.alarm_tel_not_responding()) {
			std::cerr << "telephony alarm not cleared on recovery\n";
			return 20;
		}

		m.on_coin_alive_result(false);
		if (!m.alarm_coin_hw_fault()) {
			std::cerr << "coin fault alarm missing\n";
			return 21;
		}
		m.on_coin_alive_result(true);
		if (m.alarm_coin_hw_fault()) {
			std::cerr << "coin fault alarm did not clear\n";
			return 22;
		}

		m.on_card_sensor_idle(false);
		m.on_card_sensor_idle(false);
		m.on_card_sensor_idle(false);
		if (!m.alarm_card_reader_blocked()) {
			std::cerr << "card blocked alarm missing\n";
			return 23;
		}
		m.on_card_sensor_idle(true);
		if (m.alarm_card_reader_blocked()) {
			std::cerr << "card blocked alarm did not clear\n";
			return 24;
		}
		m.on_coin_fault_sample(true);
		if (m.alarm_coin_hw_fault()) {
			std::cerr << "terminal_08 coin debounce latched too early\n";
			return 36;
		}
		m.on_coin_fault_sample(true);
		if (!m.alarm_coin_hw_fault()) {
			std::cerr << "terminal_08 coin debounce latch failed\n";
			return 37;
		}
		m.on_coin_fault_sample(false);
		if (!m.alarm_coin_hw_fault()) {
			std::cerr << "terminal_08 coin debounce cleared too early\n";
			return 38;
		}
		m.on_coin_fault_sample(false);
		if (m.alarm_coin_hw_fault()) {
			std::cerr << "terminal_08 coin debounce clear failed\n";
			return 39;
		}
		m.on_card_blocked_sample(true);
		m.on_card_blocked_sample(true);
		m.on_card_blocked_sample(true);
		if (!m.alarm_card_reader_blocked()) {
			std::cerr << "terminal_08 card blocked latch failed\n";
			return 40;
		}
		m.on_card_blocked_sample(false);
		if (!m.alarm_card_reader_blocked()) {
			std::cerr << "terminal_08 card debounce cleared too early\n";
			return 41;
		}
		m.on_card_blocked_sample(false);
		if (m.alarm_card_reader_blocked()) {
			std::cerr << "terminal_08 card debounce clear failed\n";
			return 42;
		}

		m.request_owner(peripheral_health_model::owner_coin);
		if (m.owner() != peripheral_health_model::owner_coin) {
			std::cerr << "owner grant failed\n";
			return 25;
		}
		m.request_owner(peripheral_health_model::owner_card);
		if (m.owner() != peripheral_health_model::owner_coin) {
			std::cerr << "owner should not preempt without release\n";
			return 26;
		}
		m.force_disconnect_supervision_priority();
		if (m.owner() != peripheral_health_model::owner_disconnect_supervision) {
			std::cerr << "disconnect supervision priority failed\n";
			return 27;
		}
		m.release_owner(peripheral_health_model::owner_disconnect_supervision);
		if (m.owner() != peripheral_health_model::owner_none) {
			std::cerr << "owner release failed\n";
			return 28;
		}
	}

	std::cout << "terminal_peripherals_vectors ok\n";
	return 0;
}
