// SPDX-License-Identifier: GPL-2.0-or-later
//
// Regression: WRITE opcodes must always shift 16 data bits. If EWEN has not run, the write is
// discarded but the clocks must not be interpreted as a new command stream (NCC issues WRITE
// before EWEN during first-boot EEPROM setup).

#include "millennium_microwire_93c66.h"
#include "millennium_eprm_command.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

static_assert(millennium_eprm_command_body11(0x30, 0) == 0x180);
static_assert(millennium_eprm_command_body11(0x40, 0) == 0x200);
static_assert(millennium_eprm_command_body11(0x40, 10) == 0x20a);
static_assert(millennium_eprm_command_body11(0x80, 10) == 0x40a);
// Driver SR seed maps the reference 11-bit body11 encoding to the on-wire command (see millennium_eprm_command.h).
inline constexpr std::uint16_t eprm_to_wire_body11(std::uint16_t eprm_body11)
{
	return static_cast<std::uint16_t>(0x400U | ((eprm_body11 >> 1U) & 0x3ffU));
}
static_assert(eprm_to_wire_body11(millennium_eprm_command_body11(0x40, 0)) == 0x500);
static_assert(eprm_to_wire_body11(millennium_eprm_command_body11(0x40, 10)) == 0x505);
static_assert(eprm_to_wire_body11(millennium_eprm_command_body11(0x80, 10)) == 0x605);
// Erase/write-enable opcode high nibble (0x30) merged with C — driver accepts the full 0x4C0..0x4FF wire family.
static_assert(eprm_to_wire_body11(millennium_eprm_command_body11(0x30, 0)) == 0x4c0);
static_assert(eprm_to_wire_body11(millennium_eprm_command_body11(0x30, 0x55)) == 0x4ea);

namespace {

bool di_bits_for_body11(unsigned body11, bool bits_out[11])
{
	// Invert the driver SR (START seed + 11 edges): find DI[0..10] such that (sr>>1)&0x7FF == body11.
	// Bit i is the i-th rising edge after START; same search pattern as earlier bring-up tests.
	for (std::uint32_t mask = 0; mask < (1U << 11U); ++mask) {
		std::uint32_t sr = 1U;
		for (unsigned i = 0; i < 11U; ++i) {
			bool const d = ((mask >> i) & 1U) != 0;
			sr = (sr << 1U) | (d ? 1U : 0U);
		}
		if (((sr >> 1U) & 0x7ffU) == (static_cast<std::uint32_t>(body11) & 0x7ffU)) {
			for (unsigned i = 0; i < 11U; ++i)
				bits_out[i] = ((mask >> i) & 1U) != 0;
			return true;
		}
	}
	return false;
}

void sk_pulse(millennium_microwire_93c66 &e, std::uint8_t &pb, bool di)
{
	std::uint8_t const lo = static_cast<std::uint8_t>(pb & ~0x80U);
	std::uint8_t const hi = static_cast<std::uint8_t>(lo | 0x80U);
	e.notify_port_b_clock(lo, hi, di);
	e.notify_port_b_clock(hi, lo, di);
	pb = lo;
}

bool send_command_body(millennium_microwire_93c66 &e, std::uint8_t &pb, unsigned body11)
{
	bool bits[11]{};
	if (!di_bits_for_body11(body11, bits)) {
		std::fprintf(stderr, "internal error: no DI sequence for wire body 0x%03X\n", body11 & 0x7ffU);
		return false;
	}
	sk_pulse(e, pb, true); // START
	for (unsigned i = 0; i < 11U; ++i)
		sk_pulse(e, pb, bits[i]);
	return true;
}

void send_write_payload(millennium_microwire_93c66 &e, std::uint8_t &pb, std::uint16_t data)
{
	for (int i = 15; i >= 0; --i) {
		bool const bit = ((data >> unsigned(i)) & 1U) != 0;
		sk_pulse(e, pb, bit);
	}
}

} // namespace

int main()
{
	millennium_microwire_93c66 e;
	e.reset();
	e.set_chip_select(true);
	std::uint8_t pb = 0;

	unsigned writes = 0;
	unsigned reads = 0;
	unsigned ewens = 0;
	unsigned rejected = 0;
	e.set_access_trace_cb([&](char const *op, std::uint16_t, std::uint16_t) {
		if (std::strcmp(op, "write") == 0)
			++writes;
		else if (std::strcmp(op, "read") == 0)
			++reads;
		else if (std::strcmp(op, "ewen") == 0)
			++ewens;
		else if (std::strcmp(op, "write_rejected") == 0)
			++rejected;
	});

	// WRITE word 0 (byte offset 0) before EWEN — payload must be absorbed; next command must still decode.
	if (!send_command_body(e, pb, eprm_to_wire_body11(millennium_eprm_command_body11(0x40, 0))))
		return 3;
	send_write_payload(e, pb, 0xcafeU);
	if (rejected != 1U) {
		std::fprintf(stderr, "expected 1 write_rejected for pre-EWEN write, got %u\n", rejected);
		return 1;
	}
	if (!send_command_body(e, pb, 0x44aU)) // firmware EWEN wire body
		return 3;
	if (ewens != 1U) {
		std::fprintf(stderr, "expected 1 ewen after discarded write, got %u\n", ewens);
		return 1;
	}
	if (writes != 0U) {
		std::fprintf(stderr, "write must not commit before EWEN\n");
		return 1;
	}

	if (!send_command_body(e, pb, eprm_to_wire_body11(millennium_eprm_command_body11(0x40, 10)))) // word 5
		return 3;
	send_write_payload(e, pb, 0xabcdU);
	if (writes != 1U) {
		std::fprintf(stderr, "expected 1 committed write after EWEN, got %u\n", writes);
		return 1;
	}

	if (!send_command_body(e, pb, eprm_to_wire_body11(millennium_eprm_command_body11(0x80, 10)))) // READ word 5
		return 3;
	// Match host read sequence: sample DO then toggle SK, 17 times (1 dummy + 16 data).
	std::uint16_t acc = 0;
	for (unsigned k = 0; k < 17U; ++k) {
		bool const b = e.serial_out();
		acc = static_cast<std::uint16_t>((acc << 1U) | (b ? 1U : 0U));
		sk_pulse(e, pb, false);
	}
	if (reads != 1U) {
		std::fprintf(stderr, "expected 1 read op, got %u\n", reads);
		return 1;
	}
	if (acc != 0xabcdU) {
		std::fprintf(stderr, "READ word5 got 0x%04X want 0xABCD\n", unsigned(acc));
		return 1;
	}

	// Erase/write-enable opcode path: wire image 0x4C0.. (not only the legacy 0x44a).
	e.reset();
	e.set_chip_select(true);
	pb = 0;
	writes = reads = ewens = rejected = 0;
	if (!send_command_body(e, pb, eprm_to_wire_body11(millennium_eprm_command_body11(0x30, 0))))
		return 4;
	if (ewens != 1U) {
		std::fprintf(stderr, "EPRM EWEN wire: expected 1 ewen, got %u\n", ewens);
		return 1;
	}
	if (!send_command_body(e, pb, eprm_to_wire_body11(millennium_eprm_command_body11(0x40, 20)))) // word 10
		return 4;
	send_write_payload(e, pb, 0x1234U);
	if (writes != 1U) {
		std::fprintf(stderr, "after EPRM EWEN expected 1 write, got %u\n", writes);
		return 1;
	}
	if (!send_command_body(e, pb, eprm_to_wire_body11(millennium_eprm_command_body11(0x80, 20))))
		return 4;
	acc = 0;
	for (unsigned k = 0; k < 17U; ++k) {
		bool const b = e.serial_out();
		acc = static_cast<std::uint16_t>((acc << 1U) | (b ? 1U : 0U));
		sk_pulse(e, pb, false);
	}
	if (reads != 1U) {
		std::fprintf(stderr, "EPRM EWEN path: expected 1 read, got %u\n", reads);
		return 1;
	}
	if (acc != 0x1234U) {
		std::fprintf(stderr, "READ word10 got 0x%04X want 0x1234\n", unsigned(acc));
		return 1;
	}
	return 0;
}
