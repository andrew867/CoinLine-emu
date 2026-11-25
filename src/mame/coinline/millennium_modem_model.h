// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class millennium_modem_state : std::uint8_t {
	idle,
	dialing,
	ringing,
	connected,
	busy,
	no_answer,
	carrier_lost,
	noisy_line,
};

struct millennium_modem_transcript_entry {
	char direction; // 't' = TX (toward host), 'r' = RX (from host)
	std::uint8_t byte;
};

/// ASCI-attached modem path: control-line handshake, SSI detect registers, NCC TCP bridge.
class millennium_modem_model {
public:
	void reset();
	~millennium_modem_model();

	millennium_modem_state state() const noexcept { return m_state; }
	bool dcd() const noexcept { return m_dcd; }
	bool cts() const noexcept { return m_cts; }
	bool rts() const noexcept { return m_rts; }
	bool dtr() const noexcept { return m_dtr; }

	void set_dtr(bool v) noexcept;
	void set_rts(bool v) noexcept;

	void note_tx(std::uint8_t b);
	void push_rx(std::uint8_t b);

	/// Pad/TP outpulse digit. Configured local data-call routes use the same serial path.
	void note_outpulse_digit(char digit, double now_sec);
	void poll_ncc_link(double now_sec);
	bool ncc_link_up() const noexcept { return m_ncc_up; }
	std::string const &dialed_digits() const noexcept { return m_dialed; }
	bool take_ncc_rx(std::uint8_t &out);

	/// HW_CNTL R1 connects the modem to the CO line.
	void set_line_present(bool present) noexcept { m_line_present = present; }
	void note_modem_relay(bool r1_active, double now_sec);
	/// Earlier-board PIO A modem bitbang (address/clock/RD/WR).
	char ssi_pio_a_write(std::uint8_t pio_a, bool extended_address = false);
	bool relay_active() const noexcept { return m_r1; }
	/// STATUS_PORT_2 bit 4 (MPD) level while an SSI read is in progress.
	bool ssi_mpd_level() const noexcept { return m_ssi_mpd; }

	bool inject_event(char const *name);
	void note_asci_programmed(std::uint8_t cntla, std::uint8_t cntlb) noexcept;
	bool consume_m8_pending() noexcept;
	bool m8_latched() const noexcept { return m_m8_latched; }
	bool replay_fixture_text(std::string const &text, std::string &error_out);

	std::vector<millennium_modem_transcript_entry> const &transcript() const noexcept { return m_transcript; }

	static char const *state_cstr(millennium_modem_state s) noexcept;
	static void declared_states(std::vector<millennium_modem_state> &out_states);

private:
	void goto_idle();
	void append_tx(std::uint8_t b);
	void append_rx(std::uint8_t b);
	bool advance_clean_connect_line(unsigned line_index);

	millennium_modem_state m_state = millennium_modem_state::idle;
	bool m_dcd = false;
	bool m_cts = true;
	bool m_rts = false;
	bool m_dtr = false;

	bool m_m8_pending = false;
	bool m_m8_latched = false;

	std::vector<millennium_modem_transcript_entry> m_transcript{};
	unsigned m_clean_connect_line = 0;
	bool m_in_clean_connect_replay = false;

	std::string m_dialed{};
	double m_last_digit_sec = -1.0;
	bool m_ncc_up = false;
	bool m_ncc_attempted = false;
	double m_ncc_retry_after = 0.0;
	std::intptr_t m_ncc_sock = -1;
	std::vector<std::uint8_t> m_ncc_rx{};

	void ncc_close() noexcept;
	bool ncc_connect_dialed(double now_sec);
	bool ncc_connect_host(std::uint32_t host, unsigned port, double now_sec, bool code_server);
	void ncc_send_bytes(std::uint8_t const *data, std::size_t n);
	void ncc_maybe_lab_hello();
	void flush_ncc_tx();
	void poll_call_progress(double now_sec);
	void refresh_detect_register();

	bool m_ncc_lab_hello_sent = false;
	double m_ncc_lab_hello_at = -1.0;
	std::vector<std::uint8_t> m_ncc_tx_pending{};

	// SSI modem registers selected through the port A serial interface.
	std::uint8_t m_ssi_cr0 = 0;
	std::uint8_t m_ssi_cr1 = 0;
	std::uint8_t m_ssi_cr2 = 0;
	std::uint8_t m_ssi_cr3 = 0;
	std::uint8_t m_ssi_dr = 0;
	std::uint8_t m_ssi_tr = 0;
	std::uint8_t m_ssi_pio = 0;
	std::uint8_t m_ssi_shift = 0;
	int m_ssi_bit = -1;
	bool m_ssi_reading = false;
	bool m_ssi_mpd = false;
	std::uint8_t m_ssi_write_shift = 0;
	int m_ssi_write_bits = 0;

	// Call progress after the manager connection (seconds).
	enum class cp_phase : std::uint8_t {
		idle,
		dial_tone,
		post_dial,
		ringback,
		answer_tone,
		carrier_mark,
		carrier,
	};
	cp_phase m_cp = cp_phase::idle;
	double m_ncc_connected_at = -1.0;
	bool m_r1 = false;
	bool m_line_present = false;
};
