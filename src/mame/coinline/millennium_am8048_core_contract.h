// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace coinline::tp8048 {

struct am8048_callbacks {
	std::function<std::uint8_t(unsigned port)> read_port;
	std::function<void(unsigned port, std::uint8_t value)> write_port;
	std::function<std::uint8_t(std::uint16_t addr)> read_ext_data;
	std::function<void(std::uint16_t addr, std::uint8_t value)> write_ext_data;
	std::function<bool(unsigned input)> read_test_input;
	std::function<void(std::uint8_t hgf, std::uint8_t lgf)> tone_registers_changed;
	std::function<void(char const *event, std::uint64_t cycle, std::uint8_t value)> trace;
};

struct am8048_config {
	std::uint32_t xtal_hz = 3579545;
	bool enable_timer_interrupt = true;
	bool enable_external_interrupt = true;
};

class am8048_core_contract {
public:
	virtual ~am8048_core_contract() = default;

	virtual void configure(am8048_config const &cfg, am8048_callbacks cb) = 0;
	virtual void load_program_rom(std::uint8_t const *data, std::size_t size) = 0;
	virtual void reset() = 0;
	virtual void step_cycles(std::uint64_t cycles) = 0;
	virtual std::uint64_t cycles() const = 0;

	virtual void assert_external_interrupt(bool level) = 0;
	virtual void assert_timer_interrupt(bool level) = 0;

	virtual std::uint8_t debug_port_latch(unsigned port) const = 0;
	virtual std::uint16_t debug_pc() const = 0;
};

} // namespace coinline::tp8048
