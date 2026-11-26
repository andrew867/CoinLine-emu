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
	char ssi_dtmf_digit() const noexcept {
		return (m_ssi_tr & 0x10U) && (m_ssi_cr0 & 0x02U) ? "D1234567890*#ABC"[m_ssi_tr & 0x0fU] : 0;
	}
	bool relay_active() const noexcept { return m_r1; }
	bool adsi_receiver_mode() const noexcept {
		return (m_ssi_cr0 & 0x30U) == 0x20U && (m_ssi_cr2 & 0x04U) != 0 && (m_ssi_tr & 0x10U) == 0;
	}
	void set_adsi_carrier(bool carrier) noexcept;
	/// STATUS_PORT_2 bit 4 (MPD) level while an SSI read is in progress.
	bool ssi_mpd_level() const noexcept { return m_ssi_mpd; }

	bool inject_event(char const *name);
	void note_asci_programmed(std::uint8_t cntla, std::uint8_t cntlb) noexcept;
	bool consume_m8_pending() noexcept;
	bool m8_latched() const noexcept { return m_m8_latched; }
	bool replay_fixture_text(std::string const &text, std::string &error_out);

	std::vector<millennium_modem_transcript_entry> const &transcript() const noexcept { return m_transcript; }
	// Standalone fixture tests can retain wire bytes. The running device does
	// not need an unbounded copy of each serial transfer.
	void set_transcript_enabled(bool enabled) { m_transcript_enabled = enabled; if (!enabled) m_transcript.clear(); }

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
	bool m_transcript_enabled = true;
	unsigned m_clean_connect_line = 0;
	bool m_in_clean_connect_replay = false;

	std::string m_dialed{};
	double m_last_digit_sec = -1.0;
	bool m_ncc_up = false;
	// A completed TCP handshake consumes this dial attempt, even if the
	// peer subsequently hangs up. Initial connect failures remain retryable.
	bool m_ncc_connected_this_dial = false;
	double m_ncc_retry_after = 0.0;
	std::intptr_t m_ncc_sock = -1;
	std::vector<std::uint8_t> m_ncc_rx{};

	void ncc_close() noexcept;
	bool ncc_connect_dialed(double now_sec);
	bool ncc_connect_host(std::uint32_t host, unsigned port, double now_sec, bool code_server);
	void ncc_send_bytes(std::uint8_t const *data, std::size_t n);
	void flush_ncc_tx(double now_sec);
	void lose_ncc_link(char const *event, double now_sec, int error = -1) noexcept;
	void poll_call_progress(double now_sec);
	void poll_code_training(double now_sec);
	void refresh_detect_register();
	void trace_call_progress(char const *event, double sampled_sec, int value = -1) noexcept;

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
	bool m_adsi_carrier = false;
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
		answer_gap,
		training_carrier,
		unscrambled_mark,
		s1_pattern,
		scrambled_mark,
		qam_training,
		carrier,
	};
	void set_call_phase(cp_phase phase, double now_sec);
	cp_phase m_cp = cp_phase::idle;
	double m_ncc_connected_at = -1.0;
	bool m_code_training = false;
	double m_code_phase_started = -1.0;
	double m_guest_s1_since = -1.0;
	double m_guest_s1_end = -1.0;
	double m_guest_mark_since = -1.0;
	double m_guest_qam_since = -1.0;
	bool m_code_qam = false;
	// Optional, bounded observations only; never feed back into line state.
	bool m_call_trace = false;
	unsigned m_call_trace_count = 0;
	int m_call_trace_last_dr = -1;
	double m_call_trace_poll_sec = 0.0;
	bool m_r1 = false;
	bool m_line_present = false;
};
