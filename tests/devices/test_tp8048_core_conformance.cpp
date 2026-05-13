// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_am8048_core.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <array>
#include <vector>

namespace {

coinline::tp8048::am8048_callbacks make_callbacks(std::uint8_t &p1_ext, bool &t0_in, bool &t1_in, std::vector<std::uint8_t> &bus_writes)
{
	coinline::tp8048::am8048_callbacks cb{};
	cb.read_port = [&](unsigned port) -> std::uint8_t {
		if (port == 1U)
			return p1_ext;
		return 0xffU;
	};
	cb.write_port = [&](unsigned port, std::uint8_t value) {
		if (port == 0U)
			bus_writes.push_back(value);
	};
	cb.read_test_input = [&](unsigned input) -> bool {
		if (input == 0U)
			return t0_in;
		if (input == 1U)
			return t1_in;
		return false;
	};
	return cb;
}

bool test_quasi_port_readback()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xaaU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	// MOV A,#FF; OUTL P1,A; IN A,P1; OUTL BUS,A; MOV A,#00; OUTL P1,A; IN A,P1; OUTL BUS,A; JMP 0
	std::vector<std::uint8_t> rom{ 0x23, 0xff, 0x39, 0x09, 0x02, 0x23, 0x00, 0x39, 0x09, 0x02, 0x04, 0x00 };
	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.step_cycles(40);

	if (bus_writes.size() < 2U)
		return false;
	// First read sees external pins with latch=FF, second read is forced low with latch=00.
	return bus_writes[0] == 0xaaU && bus_writes[1] == 0x00U;
}

bool test_t1_counter_edge()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = true;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	// STRT CNT; MOV A,T; OUTL BUS,A; JMP 1
	std::vector<std::uint8_t> rom{ 0x45, 0x42, 0x02, 0x04, 0x01 };
	core.load_program_rom(rom.data(), rom.size());
	core.reset();

	core.step_cycles(8);
	std::uint8_t const before = bus_writes.empty() ? 0xffU : bus_writes.back();
	t1 = false; // falling edge increments counter mode
	core.step_cycles(8);
	std::uint8_t const after = bus_writes.empty() ? 0xffU : bus_writes.back();
	return after != before;
}

bool test_idle_wakeup_on_irq()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	// EN I; IDL; NOP; JMP 2 ; vector @3 => JMP 8 ; @8 => MOV A,#5A; OUTL BUS,A; RETR
	std::vector<std::uint8_t> rom(32, 0x00U);
	rom[0] = 0x05U;
	rom[1] = 0x01U;
	rom[2] = 0x00U;
	rom[3] = 0x04U;
	rom[4] = 0x08U;
	rom[8] = 0x23U;
	rom[9] = 0x5aU;
	rom[10] = 0x02U;
	rom[11] = 0x93U;
	core.load_program_rom(rom.data(), rom.size());
	core.reset();

	core.step_cycles(6);
	std::uint16_t const idle_pc = core.debug_pc();
	core.assert_external_interrupt(true);
	core.step_cycles(20);
	core.assert_external_interrupt(false);

	bool seen_irq_write = false;
	for (std::uint8_t v : bus_writes) {
		if (v == 0x5aU) {
			seen_irq_write = true;
			break;
		}
	}
	return idle_pc != 0U && seen_irq_write;
}

bool test_jni_jf0_jf1_branches()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	// JNI/JF0/JF1 branch checks with direct bus observability.
	std::vector<std::uint8_t> rom(128, 0x00U);
	rom[0x00] = 0x86U; rom[0x01] = 0x10U; // JNI 0x10
	rom[0x02] = 0x23U; rom[0x03] = 0x11U; // MOV A,#11 (should be skipped)
	rom[0x04] = 0x02U;                     // OUTL BUS,A
	rom[0x05] = 0x04U; rom[0x06] = 0x20U; // JMP 0x20
	rom[0x10] = 0x23U; rom[0x11] = 0x22U; // MOV A,#22 (expected)
	rom[0x12] = 0x02U;                     // OUTL BUS,A
	rom[0x13] = 0x85U;                     // CLR F0
	rom[0x14] = 0x95U;                     // CPL F0 => set F0
	rom[0x15] = 0xB6U; rom[0x16] = 0x1CU; // JF0 0x1C
	rom[0x17] = 0x23U; rom[0x18] = 0x33U; // MOV A,#33 (should be skipped)
	rom[0x19] = 0x02U;                     // OUTL BUS,A
	rom[0x1A] = 0x04U; rom[0x1B] = 0x20U; // JMP 0x20
	rom[0x1C] = 0xA5U;                     // CLR F1
	rom[0x1D] = 0xB5U;                     // CPL F1 => set F1
	rom[0x1E] = 0x76U; rom[0x1F] = 0x24U; // JF1 0x24
	rom[0x20] = 0x23U; rom[0x21] = 0x44U; // MOV A,#44 (should be skipped)
	rom[0x22] = 0x02U;                     // OUTL BUS,A
	rom[0x23] = 0x04U; rom[0x24] = 0x30U; // JMP 0x30
	rom[0x24] = 0x23U; rom[0x25] = 0x55U; // MOV A,#55 (expected)
	rom[0x26] = 0x02U;                     // OUTL BUS,A
	rom[0x27] = 0x04U; rom[0x28] = 0x30U; // JMP 0x30
	rom[0x30] = 0x04U; rom[0x31] = 0x30U; // spin

	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.assert_external_interrupt(true);
	core.step_cycles(120);
	core.assert_external_interrupt(false);

	bool seen_22 = false;
	bool seen_55 = false;
	bool seen_11 = false;
	bool seen_33 = false;
	bool seen_44 = false;
	for (std::uint8_t v : bus_writes) {
		seen_22 = seen_22 || (v == 0x22U);
		seen_55 = seen_55 || (v == 0x55U);
		seen_11 = seen_11 || (v == 0x11U);
		seen_33 = seen_33 || (v == 0x33U);
		seen_44 = seen_44 || (v == 0x44U);
	}
	return seen_22 && seen_55 && !seen_11 && !seen_33 && !seen_44;
}

bool test_rotate_exchange_and_decimal_adjust()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	// RRC/RLC carry path + XCHD nibble swap + DA BCD adjust.
	std::vector<std::uint8_t> rom{
		0x23, 0x81,       // MOV A,#81
		0x97,             // CLR C
		0x67,             // RRC A  => A=40, C=1
		0x02,             // OUTL BUS,A
		0xF7,             // RLC A  => A=81, C=0
		0x02,             // OUTL BUS,A
		0xB8, 0x20,       // MOV R0,#20
		0x23, 0xAB,       // MOV A,#AB
		0xB0, 0x34,       // MOV @R0,#34
		0x30,             // XCHD A,@R0 => A=A4, [20]=3B
		0x02,             // OUTL BUS,A
		0xF0,             // MOV A,@R0
		0x02,             // OUTL BUS,A
		0x23, 0x09,       // MOV A,#09
		0x03, 0x09,       // ADD A,#09 => 12 (AC set)
		0x57,             // DA A => 18
		0x02,             // OUTL BUS,A
		0x04, 0x00        // JMP 0
	};
	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.step_cycles(200);

	bool seen_40 = false;
	bool seen_81 = false;
	bool seen_a4 = false;
	bool seen_3b = false;
	bool seen_18 = false;
	for (std::uint8_t v : bus_writes) {
		seen_40 = seen_40 || (v == 0x40U);
		seen_81 = seen_81 || (v == 0x81U);
		seen_a4 = seen_a4 || (v == 0xA4U);
		seen_3b = seen_3b || (v == 0x3BU);
		seen_18 = seen_18 || (v == 0x18U);
	}
	return seen_40 && seen_81 && seen_a4 && seen_3b && seen_18;
}

bool test_trace_event_ordering_for_idle_irq_and_stop()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	std::vector<std::string> trace_events;
	coinline::tp8048::am8048_callbacks cb = make_callbacks(p1_ext, t0, t1, bus_writes);
	cb.trace = [&](char const *event, std::uint64_t, std::uint8_t) {
		trace_events.emplace_back(event ? event : "");
	};
	core.configure({}, cb);

	// External IRQ vectors to 0x0003. After RETR the core resumes at PC=2 (byte after IDL).
	// A 2-byte JMP at 2..3 would collide with the vector, so we only place a 1-byte NOP @2 and
	// stop after one ISR — do not fall through into 0x03.. as mainline (that would mis-execute RETR).
	// STOP TCNT coverage is a separate ROM in test_stop_tcnt_emits_trace().
	std::vector<std::uint8_t> rom(32, 0x00U);
	rom[0x00] = 0x05U; // EN I
	rom[0x01] = 0x01U; // IDL
	rom[0x02] = 0x00U; // NOP (resume target)
	rom[0x03] = 0x23U; // MOV A,#A5
	rom[0x04] = 0xA5U;
	rom[0x05] = 0x02U; // OUTL BUS,A
	rom[0x06] = 0x93U; // RETR
	core.load_program_rom(rom.data(), rom.size());
	core.reset();

	core.step_cycles(10); // enter idle
	// Level-sensitive /INT: deassert after the first RETR so we do not re-enter before PC@2 advances.
	std::size_t const trace_base = trace_events.size();
	core.assert_external_interrupt(true);
	bool got_retr = false;
	for (int i = 0; i < 96 && !got_retr; ++i) {
		core.step_cycles(1);
		for (std::size_t t = trace_base; t < trace_events.size(); ++t) {
			if (trace_events[t] == "isr_return") {
				got_retr = true;
				break;
			}
		}
	}
	if (!got_retr)
		return false;
	core.assert_external_interrupt(false);
	core.step_cycles(4); // NOP @2 — do not execute ROM @3 as mainline

	bool seen_idle = false;
	bool seen_irq = false;
	bool seen_retr = false;
	std::size_t idle_idx = 0U;
	std::size_t irq_idx = 0U;
	std::size_t retr_idx = 0U;
	for (std::size_t i = 0; i < trace_events.size(); ++i) {
		if (!seen_idle && trace_events[i] == "idle_enter") {
			seen_idle = true;
			idle_idx = i;
		}
		if (!seen_irq && trace_events[i] == "irq_enter_ext") {
			seen_irq = true;
			irq_idx = i;
		}
		if (!seen_retr && trace_events[i] == "isr_return") {
			seen_retr = true;
			retr_idx = i;
		}
	}

	bool seen_a5 = false;
	for (std::uint8_t v : bus_writes) {
		if (v == 0xA5U) {
			seen_a5 = true;
			break;
		}
	}

	return seen_idle && seen_irq && seen_retr && seen_a5 && idle_idx < irq_idx && irq_idx < retr_idx;
}

bool test_stop_tcnt_emits_trace()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	std::vector<std::string> trace_events;
	coinline::tp8048::am8048_callbacks cb = make_callbacks(p1_ext, t0, t1, bus_writes);
	cb.trace = [&](char const *event, std::uint64_t, std::uint8_t) {
		trace_events.emplace_back(event ? event : "");
	};
	core.configure({}, cb);

	// 0x00: STOP TCNT; JMP 0x00
	std::vector<std::uint8_t> rom(8, 0x00U);
	rom[0x00] = 0x65U;
	rom[0x01] = 0x04U;
	rom[0x02] = 0x00U;
	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.step_cycles(8);
	for (auto const &e : trace_events) {
		if (e == "stop_tcnt")
			return true;
	}
	return false;
}

bool test_jb_bit_branches_and_dec_indirect()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	// Validate JB4 branch and DEC @R0 data-path.
	std::vector<std::uint8_t> rom(128, 0x00U);
	rom[0x00] = 0x23U; rom[0x01] = 0x10U; // MOV A,#10 (bit4 set)
	rom[0x02] = 0x92U; rom[0x03] = 0x10U; // JB4 0x10 (taken)
	rom[0x04] = 0x23U; rom[0x05] = 0x11U; // MOV A,#11 (skip)
	rom[0x06] = 0x02U;                     // OUTL BUS,A
	rom[0x10] = 0xB8U; rom[0x11] = 0x20U; // MOV R0,#20
	rom[0x12] = 0xB0U; rom[0x13] = 0x05U; // MOV @R0,#05
	rom[0x14] = 0xC0U;                     // DEC @R0 -> 04
	rom[0x15] = 0xF0U;                     // MOV A,@R0
	rom[0x16] = 0x02U;                     // OUTL BUS,A
	rom[0x17] = 0x04U; rom[0x18] = 0x17U; // JMP 0x17

	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.step_cycles(120);

	bool seen_04 = false;
	bool seen_11 = false;
	for (std::uint8_t v : bus_writes) {
		seen_04 = seen_04 || (v == 0x04U);
		seen_11 = seen_11 || (v == 0x11U);
	}
	return seen_04 && !seen_11;
}

bool test_port_immediate_logic_and_movp_tables()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	std::vector<std::uint8_t> rom(1024, 0x00U);
	// ORL/ANL BUS,#imm should update BUS latch and emit writes.
	rom[0x000] = 0x23U; rom[0x001] = 0x00U; // MOV A,#00
	rom[0x002] = 0x02U;                      // OUTL BUS,A
	rom[0x003] = 0x88U; rom[0x004] = 0x0FU;  // ORL BUS,#0F
	rom[0x005] = 0x08U;                      // INS A,BUS
	rom[0x006] = 0x02U;                      // OUTL BUS,A
	rom[0x007] = 0x98U; rom[0x008] = 0x03U;  // ANL BUS,#03
	rom[0x009] = 0x08U;                      // INS A,BUS
	rom[0x00A] = 0x02U;                      // OUTL BUS,A
	// MOVP table fetch from current page.
	rom[0x00B] = 0x23U; rom[0x00C] = 0x40U;  // MOV A,#40
	rom[0x00D] = 0xA3U;                      // MOVP A,@A  => rom[0x040]
	rom[0x00E] = 0x02U;                      // OUTL BUS,A
	// MOVP3 table fetch from page 3.
	rom[0x00F] = 0x23U; rom[0x010] = 0x22U;  // MOV A,#22
	rom[0x011] = 0xE3U;                      // MOVP3 A,@A => rom[0x322]
	rom[0x012] = 0x02U;                      // OUTL BUS,A
	rom[0x013] = 0x04U; rom[0x014] = 0x13U;  // spin
	rom[0x040] = 0x7CU;
	rom[0x322] = 0x5EU;

	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.step_cycles(220);

	bool seen_0f = false;
	bool seen_03 = false;
	bool seen_7c = false;
	bool seen_5e = false;
	for (std::uint8_t v : bus_writes) {
		seen_0f = seen_0f || (v == 0x0FU);
		seen_03 = seen_03 || (v == 0x03U);
		seen_7c = seen_7c || (v == 0x7CU);
		seen_5e = seen_5e || (v == 0x5EU);
	}
	return seen_0f && seen_03 && seen_7c && seen_5e;
}

bool test_jmpp_and_nibble_port_logic()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	core.configure({}, make_callbacks(p1_ext, t0, t1, bus_writes));

	std::vector<std::uint8_t> rom(256, 0x00U);
	// JMPP vector table dispatch.
	rom[0x00] = 0x23U; rom[0x01] = 0x20U; // MOV A,#20
	rom[0x02] = 0xB3U;                     // JMPP @A => jump to low byte from rom[0x20]
	rom[0x03] = 0x23U; rom[0x04] = 0xEEU; // should be skipped if JMPP works
	rom[0x05] = 0x02U;
	rom[0x20] = 0x40U;                     // vector low byte
	// Target @0x40: program P4 low nibble using ORLD/ANLD and observe via MOVD->BUS.
	rom[0x40] = 0x23U; rom[0x41] = 0x05U; // MOV A,#05
	rom[0x42] = 0x3CU;                     // MOVD P4,A => P4 low=5
	rom[0x43] = 0x23U; rom[0x44] = 0x0AU; // MOV A,#0A
	rom[0x45] = 0x8CU;                     // ORLD P4,A => low nibble F
	rom[0x46] = 0x0CU;                     // MOVD A,P4
	rom[0x47] = 0x02U;                     // OUTL BUS,A (expect 0F)
	rom[0x48] = 0x23U; rom[0x49] = 0x03U; // MOV A,#03
	rom[0x4A] = 0x9CU;                     // ANLD P4,A => low nibble 3
	rom[0x4B] = 0x0CU;                     // MOVD A,P4
	rom[0x4C] = 0x02U;                     // OUTL BUS,A (expect 03)
	rom[0x4D] = 0x04U; rom[0x4E] = 0x4DU; // spin

	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	core.step_cycles(240);

	bool seen_0f = false;
	bool seen_03 = false;
	bool seen_ee = false;
	for (std::uint8_t v : bus_writes) {
		seen_0f = seen_0f || (v == 0x0FU);
		seen_03 = seen_03 || (v == 0x03U);
		seen_ee = seen_ee || (v == 0xEEU);
	}
	return seen_0f && seen_03 && !seen_ee;
}

bool test_movx_roundtrip()
{
	millennium_am8048_core core{};
	std::uint8_t p1_ext = 0xffU;
	bool t0 = false;
	bool t1 = false;
	std::vector<std::uint8_t> bus_writes;
	std::array<std::uint8_t, 4096> ext_data{};
	ext_data.fill(0x00U);
	ext_data[0x044U] = 0xA7U;

	coinline::tp8048::am8048_callbacks cb = make_callbacks(p1_ext, t0, t1, bus_writes);
	cb.read_ext_data = [&](std::uint16_t addr) -> std::uint8_t {
		return ext_data[addr & 0x0fffU];
	};
	cb.write_ext_data = [&](std::uint16_t addr, std::uint8_t value) {
		ext_data[addr & 0x0fffU] = value;
	};
	core.configure({}, cb);

	// MOVX A,@R0 then MOVX @R1,A roundtrip.
	std::vector<std::uint8_t> rom{
		0x23, 0x03, // MOV A,#03
		0x3A,       // OUTL P2,A (high address nibble = 0x3)
		0xB8, 0x44, // MOV R0,#44
		0xB9, 0x55, // MOV R1,#55
		0x80,       // MOVX A,@R0 => A7
		0x02,       // OUTL BUS,A
		0x91,       // MOVX @R1,A
		0xA9,       // MOV R1,A (A still A7, just to perturb reg path)
		0x23, 0x55, // MOV A,#55
		0xA8,       // MOV R0,A
		0x80,       // MOVX A,@R0 => should read back A7 from ext[55]
		0x02,       // OUTL BUS,A
		0x04, 0x0B  // JMP 0B
	};
	core.load_program_rom(rom.data(), rom.size());
	core.reset();
	ext_data[0x044U] = 0x00U;
	ext_data[0x344U] = 0xA7U;
	core.step_cycles(220);

	bool saw_a7 = false;
	for (std::uint8_t v : bus_writes) {
		if (v == 0xA7U) {
			saw_a7 = true;
			break;
		}
	}
	return saw_a7 && ext_data[0x355U] == 0xA7U;
}

} // namespace

int main()
{
	if (!test_quasi_port_readback()) {
		std::cerr << "tp8048 core: quasi-bidirectional port readback failed\n";
		return 1;
	}
	if (!test_t1_counter_edge()) {
		std::cerr << "tp8048 core: T1 counter edge behavior failed\n";
		return 2;
	}
	if (!test_idle_wakeup_on_irq()) {
		std::cerr << "tp8048 core: idle wakeup on irq failed\n";
		return 3;
	}
	if (!test_jni_jf0_jf1_branches()) {
		std::cerr << "tp8048 core: JNI/JF0/JF1 branch behavior failed\n";
		return 4;
	}
	if (!test_rotate_exchange_and_decimal_adjust()) {
		std::cerr << "tp8048 core: rotate/xchd/da behavior failed\n";
		return 5;
	}
	if (!test_trace_event_ordering_for_idle_irq_and_stop()) {
		std::cerr << "tp8048 core: trace event ordering failed\n";
		return 6;
	}
	if (!test_stop_tcnt_emits_trace()) {
		std::cerr << "tp8048 core: stop_tcnt trace failed\n";
		return 11;
	}
	if (!test_jb_bit_branches_and_dec_indirect()) {
		std::cerr << "tp8048 core: JB/DEC indirect behavior failed\n";
		return 7;
	}
	if (!test_port_immediate_logic_and_movp_tables()) {
		std::cerr << "tp8048 core: port immediate logic / MOVP behavior failed\n";
		return 8;
	}
	if (!test_jmpp_and_nibble_port_logic()) {
		std::cerr << "tp8048 core: JMPP / ORLD / ANLD behavior failed\n";
		return 9;
	}
	if (!test_movx_roundtrip()) {
		std::cerr << "tp8048 core: MOVX behavior failed\n";
		return 10;
	}
	std::cout << "tp8048_core_conformance ok\n";
	return 0;
}
