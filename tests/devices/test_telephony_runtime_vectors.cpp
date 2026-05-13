// SPDX-License-Identifier: GPL-2.0-or-later
// Runtime vector checks for telephony periodic health behavior thresholds.

#include <cstdint>
#include <iostream>

namespace {

struct runtime_model {
	bool alarm_tel_not_responding = false;
	unsigned reset_telephony_proc_sig_count = 0;
	bool alarm_handset_discont = false;
	bool alarm_stuck_keys = false;
	unsigned miss_count = 0;
	unsigned handset_bad_count = 0;
	unsigned stuck_key_count = 0;

	// QUERY_ERROR_REPORT handling.
	void on_query_error_report(bool c4_suppressed)
	{
		if (c4_suppressed) {
			miss_count++;
			if (miss_count >= 3U) {
				alarm_tel_not_responding = true;
				reset_telephony_proc_sig_count++;
			}
			return;
		}
		miss_count = 0;
		alarm_tel_not_responding = false;
	}

	// QUERY_HANDSET_CONTINUITY handling.
	void on_query_handset(bool handset_ok)
	{
		if (handset_ok) {
			handset_bad_count = 0;
			alarm_handset_discont = false;
			return;
		}
		handset_bad_count++;
		if (handset_bad_count >= 5U)
			alarm_handset_discont = true;
	}

	// QUERY_KEY_MATRIX handling.
	void on_query_key_matrix(bool key_active)
	{
		if (!key_active) {
			stuck_key_count = 0;
			alarm_stuck_keys = false;
			return;
		}
		stuck_key_count++;
		if (stuck_key_count >= 30U)
			alarm_stuck_keys = true;
	}
};

} // namespace

int main()
{
	{
		runtime_model m;
		m.on_query_error_report(true);
		m.on_query_error_report(true);
		if (m.alarm_tel_not_responding) {
			std::cerr << "alarm raised too early for C4 suppression\n";
			return 1;
		}
		m.on_query_error_report(true);
		if (!m.alarm_tel_not_responding) {
			std::cerr << "alarm not raised after 3 suppressed C4 sweeps\n";
			return 2;
		}
		if (m.reset_telephony_proc_sig_count != 1U) {
			std::cerr << "reset signal not emitted at miss threshold\n";
			return 10;
		}
		m.on_query_error_report(false);
		if (m.alarm_tel_not_responding || m.miss_count != 0U) {
			std::cerr << "alarm/miss count not cleared on successful C4\n";
			return 3;
		}
	}

	{
		runtime_model m;
		for (unsigned i = 0; i < 4U; i++)
			m.on_query_handset(false);
		if (m.alarm_handset_discont) {
			std::cerr << "handset alarm raised before threshold\n";
			return 4;
		}
		m.on_query_handset(false);
		if (!m.alarm_handset_discont) {
			std::cerr << "handset alarm not raised at threshold\n";
			return 5;
		}
		m.on_query_handset(true);
		if (m.alarm_handset_discont || m.handset_bad_count != 0U) {
			std::cerr << "handset alarm not cleared on recovery\n";
			return 6;
		}
	}

	{
		runtime_model m;
		for (unsigned i = 0; i < 29U; i++)
			m.on_query_key_matrix(true);
		if (m.alarm_stuck_keys) {
			std::cerr << "stuck-key alarm raised before threshold\n";
			return 7;
		}
		m.on_query_key_matrix(true);
		if (!m.alarm_stuck_keys) {
			std::cerr << "stuck-key alarm not raised at threshold\n";
			return 8;
		}
		m.on_query_key_matrix(false);
		if (m.alarm_stuck_keys || m.stuck_key_count != 0U) {
			std::cerr << "stuck-key alarm not cleared by idle matrix\n";
			return 9;
		}
	}

	std::cout << "telephony_runtime_vectors ok\n";
	return 0;
}
