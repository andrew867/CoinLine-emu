// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

// Phone-line jack on 127.0.0.1. A central-office program connects here.
// COINLINE_LINE_PORT selects the port. Unset or 0 leaves the jack closed.
//
// From the central office, one command per line:
//   R1 / R0   ring-detect input asserted / released
//   C1 / C0   line connected / open interval
//   V0 / V1   line-reversal phase 0 / phase 1
//   V         both reversal phases released
//   T1 / T0   following audio is CO dial tone / normal line audio
//   W<hex>    8 kHz signed 16-bit little-endian samples toward the handset
// To the central office:
//   H1 / H0   handset off-hook / on-hook
//   E1 / E0   firmware ringer-enable bit
//   S / X     loop seized / released
//   D<digit>  digit the firmware outpulsed
//   M<hex>    8 kHz signed 16-bit little-endian samples from the handset microphone
class millennium_line_jack {
public:
	void open_from_env();
	void close() noexcept;
	void poll();
	void send_line(char const *text);
	bool ring_present() const noexcept { return m_ring; }
	bool enabled() const noexcept { return m_listen != -1; }
	bool line_connected() const noexcept { return m_connected; }
	bool co_dial_tone() const noexcept { return m_co_dial_tone; }
	// 0 none, 1 reversal phase 0, 2 reversal phase 1.
	int reversal() const noexcept { return m_reversal; }
	bool take_connected() noexcept;
	// Handset microphone, 8 kHz. Dropped when the central office falls behind.
	void push_outgoing_pcm(std::int16_t sample);
	// One sample the central office sent, or false when the queue is empty.
	bool take_incoming_pcm(std::int16_t &sample);

private:
	void flush_outgoing_pcm();
	void ingest_pcm_line(std::string const &hex);
	void drain_tx();

	std::intptr_t m_listen = -1;
	std::intptr_t m_client = -1;
	std::string m_rx;
	std::string m_tx_pending;
	bool m_ring = false;
	bool m_connected = false;
	bool m_co_dial_tone = false;
	int m_reversal = 0;
	bool m_connected_edge = false;
	std::mutex m_pcm_mu;
	std::deque<std::int16_t> m_pcm_to_co;
	std::deque<std::int16_t> m_pcm_from_co;
};
