// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coinline::terminal {

class ring_alerter_model {
public:
	enum disable_reason : std::uint8_t {
		reason_voice = 0x01,
		reason_data = 0x02,
		reason_terminal = 0x04,
	};

	void disable(std::uint8_t reason) noexcept;
	void enable(std::uint8_t reason) noexcept;
	void on_ring_on() noexcept;
	void on_ring_abandoned() noexcept;
	void on_ring_timeout() noexcept;

	bool enring_asserted() const noexcept { return m_disable_mask == 0U; }
	bool incoming_ring_emitted() const noexcept { return m_incoming_emitted; }
	bool ring_abandoned_emitted() const noexcept { return m_abandoned_emitted; }
	unsigned ring_pulse_count() const noexcept { return m_ring_pulse_count; }
	void clear_events() noexcept;

private:
	std::uint8_t m_disable_mask = 0U;
	bool m_is_ringing = false;
	bool m_incoming_emitted = false;
	bool m_abandoned_emitted = false;
	unsigned m_ring_pulse_count = 0U;
};

class forgotten_card_alarm_model {
public:
	void start(unsigned on_delay_ticks, unsigned cadence_on_ticks, unsigned cadence_off_ticks, unsigned total_ticks) noexcept;
	void cancel() noexcept;
	void tick() noexcept;
	void acknowledge_user_return() noexcept;

	bool tone_on() const noexcept { return m_tone_on; }
	bool relay_warning_path() const noexcept { return m_relay_warning; }
	bool active() const noexcept { return m_active || m_armed; }
	unsigned elapsed_ticks() const noexcept { return m_t; }

private:
	unsigned m_on_delay = 0U;
	unsigned m_cadence_on = 0U;
	unsigned m_cadence_off = 0U;
	unsigned m_total = 0U;
	unsigned m_t = 0U;
	unsigned m_cadence_t = 0U;
	bool m_armed = false;
	bool m_active = false;
	bool m_tone_on = false;
	bool m_relay_warning = false;
};

class data_jack_model {
public:
	enum relay_state : std::uint8_t {
		relay_idle = 0,
		relay_connecting_modem_sink = 1,
		relay_dialing_completed = 2,
		relay_disconnecting_modem_sink = 3,
		relay_connected_data_active = 4,
		relay_disconnect_delay = 5,
	};

	void manual_keypad_digit_signal() noexcept;
	void on_relay_sequence_complete_event() noexcept;
	void on_dialing_complete_event() noexcept;
	void on_laptop_drop() noexcept;
	void on_data_session_established() noexcept;
	void on_disconnect_delay_expired() noexcept;

	relay_state state() const noexcept { return m_state; }
	bool control_on() const noexcept { return m_control_on; }
	bool call_established() const noexcept { return m_call_established; }
	bool data_session_active() const noexcept { return m_data_session_active; }
	bool disconnect_supervision_pending() const noexcept { return m_disconnect_supervision_pending; }

private:
	relay_state m_state = relay_idle;
	bool m_control_on = false;
	bool m_call_established = false;
	bool m_data_session_active = false;
	bool m_disconnect_supervision_pending = false;
};

class eeprom_model {
public:
	explicit eeprom_model(std::size_t size) : m_bytes(size, 0U) {}

	bool read(std::size_t offset, std::size_t size, std::vector<std::uint8_t> &out) const noexcept;
	bool write(std::size_t offset, std::vector<std::uint8_t> const &data) noexcept;
	bool verify(std::size_t offset, std::vector<std::uint8_t> const &data) const noexcept;
	bool write_record_with_checksum(std::size_t offset, std::vector<std::uint8_t> const &payload) noexcept;
	bool read_record_with_checksum(std::size_t offset, std::size_t payload_size, std::vector<std::uint8_t> &payload_out) const noexcept;

private:
	std::vector<std::uint8_t> m_bytes;
};

/// Mirrors firmware collection-status flags (\c CBXST_* constants below). **Physical** vault/cover bits are
/// driven through MACH port \c H reads (\c millennium_cashbox_hw); this struct is behavioral bookkeeping only.
class cashbox_collection_model {
public:
	struct record_payload {
		unsigned box_id = 0U;
		unsigned sequence = 0U;
		std::uint8_t status = 0U;
	};

	static constexpr std::uint8_t cbxst_new_box = 0x01U;
	static constexpr std::uint8_t cbxst_new_status = 0x02U;
	static constexpr std::uint8_t cbxst_coll_dlog_req = 0x04U;

	void commit_record(unsigned box_id, bool upload_required) noexcept;
	std::uint8_t status() const noexcept { return m_status; }
	unsigned sequence() const noexcept { return m_seq; }
	unsigned current_box() const noexcept { return m_box; }
	record_payload last_payload() const noexcept { return m_last_payload; }

private:
	std::uint8_t m_status = 0U;
	unsigned m_seq = 0U;
	unsigned m_box = 0U;
	bool m_box_set = false;
	record_payload m_last_payload{};
};

class peripheral_health_model {
public:
	enum shared_owner : std::uint8_t {
		owner_none = 0,
		owner_coin = 1,
		owner_card = 2,
		owner_data_jack = 3,
		owner_disconnect_supervision = 4,
	};

	void on_telephony_alive_result(bool ok) noexcept;
	void on_coin_alive_result(bool ok) noexcept;
	void on_card_sensor_idle(bool idle) noexcept;
	void on_coin_fault_sample(bool fault) noexcept;
	void on_card_blocked_sample(bool blocked) noexcept;

	void request_owner(shared_owner owner) noexcept;
	void force_disconnect_supervision_priority() noexcept;
	void release_owner(shared_owner owner) noexcept;

	bool alarm_tel_not_responding() const noexcept { return m_alarm_tel_not_responding; }
	bool alarm_coin_hw_fault() const noexcept { return m_alarm_coin_hw_fault; }
	bool alarm_card_reader_blocked() const noexcept { return m_alarm_card_reader_blocked; }
	shared_owner owner() const noexcept { return m_owner; }

private:
	unsigned m_tel_miss = 0U;
	unsigned m_card_blocked = 0U;
	unsigned m_coin_fault_consecutive = 0U;
	unsigned m_coin_recover_consecutive = 0U;
	unsigned m_card_recover_consecutive = 0U;
	bool m_alarm_tel_not_responding = false;
	bool m_alarm_coin_hw_fault = false;
	bool m_alarm_card_reader_blocked = false;
	shared_owner m_owner = owner_none;
};

} // namespace coinline::terminal
