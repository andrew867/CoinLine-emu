// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <deque>
#include <optional>
#include <string>
#include <vector>

struct millennium_smart_fixture {
	std::string protocol = "memory";
	std::vector<std::uint8_t> atr{};
	std::vector<std::uint8_t> memory{};
	unsigned atr_delay_us = 4000;
	unsigned apdu_response_delay_us = 1000;
	unsigned inter_character_delay_us = 0;
	bool requires_authorization = false;
	std::string async_application = "none";
	bool async_internal_reset = false;
	// Optional, bounded test memory programming. The contact-test card is read-only.
	bool sync_program_enabled = false;
	unsigned sync_program_first_bit = 0;
	unsigned sync_program_end_bit = 0; // exclusive
	unsigned sync_program_us = 10000;
};

// Port H gates the asynchronous side; D selects the physical contact.
inline std::optional<unsigned> millennium_async_contact_selector(std::uint8_t h, std::uint8_t d)
{
	return (h & 0x08U) ? std::nullopt : std::optional<unsigned>(d & 0x03U);
}

class millennium_smartcard_model {
public:
	struct serial_parameters { unsigned fi = 372; unsigned di = 1; };
	enum class negotiated_protocol : std::uint8_t {
		none = 0,
		t0,
		t1,
		sync_fallback,
	};

	bool parse_fixture_json(std::string const &json_text, std::string &error_out);

	void reset_session();
	void insert_card_at(std::uint64_t cycle, std::uint64_t cpu_hz);
	void remove_card();

	void notify_reset(std::uint64_t cycle);
	std::uint8_t status_lines() const;

	// Synchronous contact-card interface. The board drives real power,
	// reset and clock pins; the card exposes one bit, not a byte FIFO.
	void update_later_pins(std::uint8_t port_d, std::uint8_t port_e, std::uint8_t port_f, std::uint64_t cycle = 0);
	void update_earlier_pins(std::uint8_t port_a, std::uint8_t port_c, std::uint64_t cycle = 0);
	bool sync_data_selected() const noexcept { return m_sync_selected; }
	bool sync_data_high() const noexcept;
	bool sync_powered() const noexcept { return m_sync_powered; }
	std::size_t sync_bit_address() const noexcept { return m_sync_bit_address; }
	bool sync_test_memory_writable() const noexcept { return m_fixture.protocol == "sync" && m_fixture.sync_program_enabled; }
	bool restore_sync_test_memory(std::vector<std::uint8_t> const &memory);

	// Later-board physical serial selection and activation. The UART is
	// serviced separately by the device; these methods operate on characters
	// only after they have crossed the selected contact.
	void update_async_later_pins(std::uint8_t h, std::uint8_t d, std::uint8_t e, std::uint64_t cycle);
	void update_async_contact_pins(bool selected, bool power, bool clock, bool reset, std::uint64_t cycle);
	bool async_routed() const noexcept { return m_async_routed; }
	bool async_active() const noexcept { return m_async_active; }
	bool async_inverse() const noexcept { return m_fixture.protocol == "async" && !m_fixture.atr.empty() && m_fixture.atr[0] == 0x3f; }
	unsigned async_inter_character_delay_us() const noexcept { return m_fixture.inter_character_delay_us; }
	bool async_lab_application() const noexcept { return m_fixture.protocol == "async" && m_fixture.async_application == "lab-readonly"; }
	std::optional<std::uint8_t> async_read_byte(std::uint64_t cycle);
	std::optional<std::uint64_t> async_next_ready_cycle() const noexcept {
		return m_async_active && !m_async_tx.empty() ? std::optional<std::uint64_t>(m_async_ready_cycle) : std::nullopt;
	}
	void async_write_byte(std::uint8_t data, std::uint64_t cycle);
	std::optional<serial_parameters> async_take_parameters();

	std::uint8_t read_fifo(std::uint64_t cycle);
	void write_command(std::uint8_t b, std::uint64_t cycle);
	bool negotiate_protocol(bool allow_sync_probe);
	void authorize_session(bool authorized) noexcept { m_authorized = authorized; }

	std::size_t atr_bytes_remaining() const noexcept { return m_atr_pending.size(); }
	std::vector<std::uint8_t> const &memory() const noexcept { return m_fixture.memory; }
	std::string const &protocol() const noexcept { return m_fixture.protocol; }
	negotiated_protocol selected_protocol() const noexcept { return m_selected_protocol; }

private:
	void schedule_atr(std::uint64_t cycle);
	void drive_sync_pins(bool selected, bool power, bool reset, bool clock, bool released, std::uint64_t cycle);
	void clear_sync_session();
	void finish_sync_program(std::uint64_t cycle);
	void clear_async_session();
	void activate_async(std::uint64_t cycle);
	void queue_async(std::vector<std::uint8_t> const &bytes, std::uint64_t cycle, unsigned delay_us);
	void async_t0_byte(std::uint8_t data, std::uint64_t cycle);
	void async_t1_byte(std::uint8_t data, std::uint64_t cycle);
	void async_pps_byte(std::uint8_t data, std::uint64_t cycle);
	std::vector<std::uint8_t> application_apdu(std::vector<std::uint8_t> const &apdu);
	std::vector<std::uint8_t> t1_block(std::uint8_t pcb, std::vector<std::uint8_t> const &inf) const;
	std::vector<std::uint8_t> t1_next_response();

	millennium_smart_fixture m_fixture{};
	std::deque<std::uint8_t> m_rx{};
	std::vector<std::uint8_t> m_atr_pending{};
	bool m_card_present = false;
	bool m_reset_pending = false;
	std::uint64_t m_atr_ready_cycle = 0;
	std::uint64_t m_block_until_cycle = 0;
	bool m_atr_enqueued = false;
	std::uint64_t m_cpu_hz = 12288000;
	unsigned m_read_addr = 0;
	bool m_cmd_phase = false;
	std::uint8_t m_cmd0 = 0;
	negotiated_protocol m_selected_protocol = negotiated_protocol::none;
	bool m_authorized = false;
	bool m_sync_selected = false;
	bool m_sync_powered = false;
	bool m_sync_reset = false;
	bool m_sync_clock = false;
	bool m_sync_released = true;
	bool m_sync_reset_clocked = false;
	bool m_sync_ready = false;
	bool m_earlier_power_latch = false;
	bool m_earlier_power_clock = false;
	bool m_later_power_latch = false;
	bool m_later_power_clock = false;
	std::size_t m_sync_bit_address = 0;
	bool m_sync_program_armed = false;
	bool m_sync_program_active = false;
	std::uint64_t m_sync_program_start = 0;
	bool m_async_routed = false;
	bool m_async_active = false;
	bool m_async_reset = false;
	bool m_async_selected_application = false;
	std::deque<std::uint8_t> m_async_tx{};
	std::uint64_t m_async_ready_cycle = 0;
	std::vector<std::uint8_t> m_async_command{};
	std::vector<std::uint8_t> m_pps_request{};
	std::optional<serial_parameters> m_pending_parameters{};
	bool m_pps_possible = true;
	std::size_t m_t0_incoming_length = 0;
	std::vector<std::uint8_t> m_t1_chain{};
	std::vector<std::uint8_t> m_t1_last_request{};
	std::vector<std::uint8_t> m_t1_last_reply{};
	std::vector<std::uint8_t> m_t1_outgoing{};
	unsigned m_t1_ifsd = 32;
	bool m_t1_rx_sequence = false;
	bool m_t1_tx_sequence = false;
};

// Independent internal contact slots. Board-specific power/reset/clock
// decoding is external to this model; customer contact pins are not SAM pins.
class millennium_sam_model {
public:
	bool configure_lab_slot(unsigned slot, std::string const &fixture, std::uint64_t cycle,
		std::uint64_t cpu_hz, std::string &error);
	void remove(unsigned slot, std::uint64_t cycle);
	void select(std::optional<unsigned> selector, std::uint64_t cycle);
	void pins(unsigned slot, bool power, bool clock, bool reset, std::uint64_t cycle);
	void later_pins(std::uint8_t h, std::uint8_t d, std::uint8_t e, std::uint8_t c,
		std::uint8_t g, std::uint64_t cycle);
	bool active() const noexcept;
	millennium_smartcard_model *active_card() noexcept;
	std::optional<std::uint8_t> read(std::uint64_t cycle);
	void write(std::uint8_t data, std::uint64_t cycle);
	std::optional<unsigned> selected() const noexcept { return m_selected; }

private:
	struct socket {
		millennium_smartcard_model card;
		bool power = false;
		bool clock = false;
		bool reset = false;
	};
	void update(unsigned slot, std::uint64_t cycle);
	std::array<socket, 3> m_slots{};
	std::optional<unsigned> m_selected;
};
