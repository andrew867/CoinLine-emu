// SPDX-License-Identifier: GPL-2.0-or-later
// NCC runtime session vectors: ACK/NACK, retry, and timeout thresholds.

#include "millennium_modem_ncc.h"

#include <iostream>

int main()
{
	using namespace coinline::modem::ncc;

	session_model s{};
	s.config.max_retries = 3U;

	{
		ncc_frame_fields tx{};
		tx.control = 0x01U;
		s.on_tx_frame(tx.control);
		if (s.state != session_state::waiting_ack) {
			std::cerr << "state not waiting_ack after tx\n";
			return 1;
		}
		ncc_frame_fields ack{};
		ack.control = static_cast<std::uint8_t>(CONTROL_ACK | 0x01U);
		s.on_rx_frame(ack);
		if (s.state != session_state::established || s.retry_count != 0U) {
			std::cerr << "ack did not establish session\n";
			return 2;
		}
	}

	{
		ncc_frame_fields tx{};
		tx.control = 0x02U;
		s.on_tx_frame(tx.control);
		ncc_frame_fields nack{};
		nack.control = static_cast<std::uint8_t>(CONTROL_NACK | 0x02U);
		s.on_rx_frame(nack);
		if (s.state != session_state::waiting_ack || !s.last_frame_was_nack) {
			std::cerr << "nack did not keep session in waiting_ack\n";
			return 3;
		}
		s.on_ack_timeout();
		s.on_ack_timeout();
		if (s.state == session_state::failed) {
			std::cerr << "session failed too early\n";
			return 4;
		}
		s.on_ack_timeout();
		if (s.state != session_state::failed) {
			std::cerr << "session not failed after timeout threshold\n";
			return 5;
		}
	}

	{
		// Clear-call frames force the model back to idle.
		ncc_frame_fields clear{};
		clear.control = CONTROL_CLEAR_CALL;
		s.on_rx_frame(clear);
		if (s.state != session_state::idle) {
			std::cerr << "clear-call did not return session to idle\n";
			return 6;
		}
	}

	std::cout << "modem_ncc_session_vectors ok\n";
	return 0;
}
