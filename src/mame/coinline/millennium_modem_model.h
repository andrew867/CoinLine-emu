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

/// ASCI-attached modem path: control-line handshake, transcript, and fixture replay. A full remote-access **dialer**
/// (V.22/V.22bis carrier timers, equalizer sequencing, etc.) is **not** simulated — those live in host firmware and
/// host-side scenario scripts; use \c inject_event / fixtures for line behaviour at test granularity.
class millennium_modem_model {
public:
	void reset();

	millennium_modem_state state() const noexcept { return m_state; }
	bool dcd() const noexcept { return m_dcd; }
	bool cts() const noexcept { return m_cts; }
	bool rts() const noexcept { return m_rts; }
	bool dtr() const noexcept { return m_dtr; }

	void set_dtr(bool v) noexcept;
	void set_rts(bool v) noexcept;

	// Firmware / UART model: record bytes written (TX) and bytes received (RX) from the wire.
	void note_tx(std::uint8_t b);
	void push_rx(std::uint8_t b);

	// Injected modem / line events (scenario / tests).
	bool inject_event(char const *name);

	// Record ASCI init (M8) — call when programming is first observed.
	void note_asci_programmed(std::uint8_t cntla, std::uint8_t cntlb) noexcept;
	bool consume_m8_pending() noexcept;
	bool m8_latched() const noexcept { return m_m8_latched; }

	// Parse fixtures/modem/*.hex: # comments, # event <name> lines, hex byte lines.
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
};
