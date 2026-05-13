// SPDX-License-Identifier: GPL-2.0-or-later
// Telephony boot contract vectors: transport-valid C0 is not semantic readiness by itself.

#include <array>
#include <cstdint>
#include <iostream>

namespace {

enum class boot_state : std::uint8_t {
	reset_pending = 0,
	acked_not_ready,
	status_ready,
	runtime_active,
	timeout_latched,
};

struct boot_contract_model {
	boot_state state = boot_state::reset_pending;
	bool power_ack = false;
	bool hook = false;
	bool power = false;
	bool error = false;
	bool status = false;
	bool ready = false;
	unsigned runtime_miss_count = 0;
	bool alarm = false;

	void observe_byte(std::uint8_t b)
	{
		if (b == 0x70U || b == 0x72U) {
			power_ack = true;
			state = boot_state::acked_not_ready;
		} else if (b == 0x6CU || b == 0x6EU) {
			hook = true;
		} else if (b == 0x7AU || b == 0x7CU) {
			power = true;
		}
		update();
	}

	void observe_error_report(std::uint8_t payload)
	{
		error = (payload == 0x00U);
		if (error) {
			runtime_miss_count = 0;
			alarm = false;
		}
		update();
	}

	void observe_status(std::array<std::uint8_t, 5> payload)
	{
		// Healthy idle: zero counters, zero call duration, idle call-state byte.
		status = payload == std::array<std::uint8_t, 5>{ 0, 0, 0, 0, 0 };
		update();
	}

	void runtime_health_sweep(bool c4_present)
	{
		if (c4_present) {
			runtime_miss_count = 0;
			alarm = false;
			return;
		}
		runtime_miss_count++;
		if (runtime_miss_count >= 3U) {
			alarm = true;
			state = boot_state::timeout_latched;
			ready = false;
		}
	}

	void update()
	{
		if (power_ack && hook && power && error && status) {
			state = boot_state::runtime_active;
			ready = true;
		} else if (status) {
			state = boot_state::status_ready;
		}
	}
};

} // namespace

int main()
{
	{
		boot_contract_model m;
		m.observe_error_report(0x00U);
		m.observe_status({ 0, 0, 0, 0, 0 });
		if (m.ready) {
			std::cerr << "C4/C0 without power/hook bundle must not imply readiness\n";
			return 1;
		}
	}

	{
		boot_contract_model m;
		m.observe_byte(0x70U); // POWER_ON_RESET on-hook idle
		m.observe_byte(0x6CU);
		m.observe_byte(0x7AU);
		m.observe_error_report(0x00U);
		m.observe_status({ 0, 0, 0, 0, 0 });
		if (!m.ready || m.state != boot_state::runtime_active) {
			std::cerr << "complete boot bundle did not reach runtime-active state\n";
			return 2;
		}
	}

	{
		boot_contract_model m;
		m.observe_byte(0x70U);
		m.observe_byte(0x6CU);
		m.observe_byte(0x7AU);
		m.observe_error_report(0x00U);
		m.observe_status({ 0, 0, 0, 0, 0x40U });
		if (m.ready) {
			std::cerr << "sync-error status payload must fail healthy-readiness semantics\n";
			return 3;
		}
	}

	{
		boot_contract_model m;
		m.observe_byte(0x70U);
		m.observe_byte(0x6CU);
		m.observe_byte(0x7AU);
		m.observe_error_report(0x00U);
		m.observe_status({ 0, 0, 0, 0, 0 });
		m.runtime_health_sweep(false);
		m.runtime_health_sweep(false);
		if (m.alarm) {
			std::cerr << "runtime watchdog alarm raised before third miss\n";
			return 4;
		}
		m.runtime_health_sweep(false);
		if (!m.alarm || m.ready) {
			std::cerr << "runtime watchdog did not latch timeout on third miss\n";
			return 5;
		}
		m.runtime_health_sweep(true);
		if (m.alarm || m.runtime_miss_count != 0U) {
			std::cerr << "runtime watchdog did not clear on valid error report\n";
			return 6;
		}
	}

	std::cout << "telephony_boot_contract_vectors ok\n";
	return 0;
}
