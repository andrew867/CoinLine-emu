// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

/// Raw external equipment monitor and documented relay output combinations.
/// This does not decide whether a call is allowed or create a data session.
class data_jack_hardware {
public:
	enum class profile : std::uint8_t { disabled, downloaded, desktop, universal };
	enum class route : std::uint8_t { unknown, laptop_and_network, laptop_only, network_only };
	void configure(profile wiring, bool monitor_active_high) noexcept;
	void reset() noexcept;
	void set_connector(bool plugged, bool seized) noexcept;
	void set_office_line(bool available) noexcept { m_office_line_available = available; }
	void write_pio_c(std::uint8_t value) noexcept { m_pio_c = value; }
	void write_hw_control(std::uint8_t value) noexcept { m_hw_control = value; }
	bool enabled() const noexcept { return m_profile != profile::disabled; }
	bool plugged() const noexcept { return m_plugged; }
	bool external_loop() const noexcept { return m_plugged && m_seized; }
	bool office_line_available() const noexcept { return m_office_line_available; }
	std::uint8_t merge_monitor(std::uint8_t status) const noexcept;
	route selected_route() const noexcept;
	std::uint8_t raw_relay_mask() const noexcept { return m_pio_c & 0xc0U; }
	std::uint8_t raw_internal_relays() const noexcept { return m_hw_control & 7U; }
	bool dtmf_attenuated() const noexcept { return (m_pio_c & 0x10U) != 0U; }
	static char const *route_name(route selected) noexcept;
private:
	profile m_profile = profile::disabled;
	bool m_monitor_active_high = true;
	bool m_plugged = false;
	bool m_seized = false;
	bool m_office_line_available = false;
	std::uint8_t m_pio_c = 0;
	std::uint8_t m_hw_control = 0;
};

// Observe completed firmware-generated positive watchdog pulses. Times are
// emulated nanoseconds; the one-second service deadline is not a measured
// supervisor IC threshold.
class watchdog_monitor {
public:
	static constexpr std::uint64_t deadline_ns = 1000000000ULL;
	struct pulse { std::uint64_t at, width, gap; bool has_gap; };
	void reset(bool high) noexcept {
		m_high = high; m_have_rise = false; m_have_pulse = false;
		m_rise = 0; m_last = 0;
	}
	std::optional<pulse> observe(bool high, std::uint64_t now) noexcept {
		if (high == m_high) return std::nullopt;
		m_high = high;
		if (high) { m_rise = now; m_have_rise = true; return std::nullopt; }
		if (!m_have_rise) return std::nullopt;
		m_have_rise = false;
		pulse const result{now, now - m_rise, m_have_pulse ? now - m_last : 0, m_have_pulse};
		m_have_pulse = true; m_last = now;
		return result;
	}
private:
	bool m_high = false, m_have_rise = false, m_have_pulse = false;
	std::uint64_t m_rise = 0, m_last = 0;
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
