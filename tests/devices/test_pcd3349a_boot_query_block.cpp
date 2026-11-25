// SPDX-License-Identifier: GPL-2.0-or-later
// PCD3349A contract: boot ACK stays on the CP TX queue, and the power-on query
// block is answered by the 8048 ROM (hook, power, C4 len 4, C0 len 8).

#include "millennium_am8048_core.h"
#include "millennium_pcd3349a_contract.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr char controller_rom[] = COINLINE_EMU_SOURCE_DIR "/firmware/tp_pcd3349a_behavioural.rom";

std::string hex_dump(std::vector<std::uint8_t> const &bytes)
{
	std::string out;
	for (std::uint8_t b : bytes) {
		char buf[8];
		std::snprintf(buf, sizeof(buf), "%02X ", unsigned(b));
		out += buf;
	}
	return out;
}

bool load_rom(std::vector<std::uint8_t> &rom)
{
	std::ifstream in(controller_rom, std::ios::binary);
	if (!in.good()) {
		std::cerr << "Cannot read the assembled telephony controller ROM\n";
		return false;
	}
	rom.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	if (rom.size() < 4096U) {
		std::cerr << "rom short: " << rom.size() << "\n";
		return false;
	}
	rom.resize(4096U);
	return true;
}

std::vector<std::uint8_t> drain_tx(coinline::tp8048::pcd3349a_contract &tp)
{
	std::vector<std::uint8_t> out;
	while (tp.has_tx_byte())
		out.push_back(tp.pop_tx_byte());
	return out;
}

bool ui_contains(coinline::tp8048::pcd3349a_contract &tp, std::uint8_t want)
{
	bool found = false;
	while (tp.has_ui_event()) {
		if (tp.pop_ui_event() == want)
			found = true;
	}
	return found;
}

bool boot_ack_on_tx(bool on_hook, std::uint8_t expect, std::vector<std::uint8_t> const &rom)
{
	millennium_am8048_core core;
	coinline::tp8048::pcd3349a_contract tp(core);
	tp.load_firmware_rom(rom.data(), rom.size());
	tp.reset();
	coinline::tp8048::tp_input_snapshot snap{};
	snap.linectrl = 0x01U;
	snap.keymatrix = on_hook ? 0U : 0x00080000U;
	tp.set_input_snapshot(snap);
	tp.step_tp_cycles_raw(400000ULL);
	tp.flush_boot_tx_queue();
	if (!tp.has_tx_byte()) {
		std::cerr << (on_hook ? "on-hook" : "off-hook") << " boot ACK missing from has_tx_byte()\n";
		return false;
	}
	std::uint8_t const ack = tp.pop_tx_byte();
	if (ack != expect) {
		std::cerr << (on_hook ? "on-hook" : "off-hook") << " boot byte 0x" << std::hex << unsigned(ack)
				  << " expected 0x" << unsigned(expect) << std::dec << "\n";
		return false;
	}
	if (ui_contains(tp, ack)) {
		std::cerr << "boot ACK was classified as a UI event\n";
		return false;
	}
	// A controller reset during a running CP session starts a fresh local
	// clock epoch; it must not replay all the CP cycles since machine boot.
	std::uint64_t const origin = 900000000ULL;
	tp.rebase_cp_clock(origin);
	std::uint64_t const start = core.cycles();
	tp.run_until_cp_cycle(origin, 12288000U);
	if (core.cycles() != start)
		return false;
	tp.run_until_cp_cycle(origin + 122880ULL, 12288000U);
	std::uint64_t const advanced = core.cycles() - start;
	if (advanced < 35000ULL || advanced > 36000ULL)
		return false;
	return true;
}

bool framed(std::vector<std::uint8_t> const &bytes, std::uint8_t type, std::uint8_t len)
{
	if (len < 3U)
		return false;
	for (std::size_t i = 0; i + len <= bytes.size(); ++i) {
		if (bytes[i] != type || bytes[i + 1] != len)
			continue;
		unsigned sum = unsigned(type) + unsigned(len);
		for (std::uint8_t n = 0; n < static_cast<std::uint8_t>(len - 3U); ++n)
			sum += bytes[i + 2U + n];
		if ((sum & 0xffU) == bytes[i + len - 1U])
			return true;
	}
	return false;
}

bool has_byte(std::vector<std::uint8_t> const &bytes, std::uint8_t v)
{
	for (std::uint8_t b : bytes)
		if (b == v)
			return true;
	return false;
}

bool query_block(std::vector<std::uint8_t> const &rom)
{
	millennium_am8048_core core;
	coinline::tp8048::pcd3349a_contract tp(core);
	tp.load_firmware_rom(rom.data(), rom.size());
	tp.reset();
	coinline::tp8048::tp_input_snapshot snap{};
	snap.linectrl = 0x00U;
	tp.set_input_snapshot(snap);
	tp.step_tp_cycles_raw(400000ULL);
	tp.flush_boot_tx_queue();
	std::vector<std::uint8_t> got = drain_tx(tp);
	if (!has_byte(got, 0x70U) && !has_byte(got, 0x72U)) {
		std::cerr << "query setup missing boot ACK: " << hex_dump(got) << "\n";
		return false;
	}
	// Startup arms FLAGS1.F_RESERVED, so the first 0x33 may be 0x7C once.
	// The CP records a long power loss and still accepts the following 0xC0.
	std::uint8_t const cmds[] = {0x30U, 0x33U, 0x38U, 0x31U};
	for (std::uint8_t cmd : cmds) {
		tp.receive_cp_byte(cmd);
		tp.drain_tx_ring();
		// HOOK_STATE_DELAY_IRQS timer overflows before the steady 0x6C/0x6E byte.
		tp.step_tp_cycles_raw(80000ULL);
		auto const part = drain_tx(tp);
		got.insert(got.end(), part.begin(), part.end());
	}
	bool const hook = has_byte(got, 0x6cU) || has_byte(got, 0x6eU);
	bool const power = has_byte(got, 0x7aU) || has_byte(got, 0x7cU);
	bool const c4 = framed(got, 0xc4U, 0x04U);
	bool const c0 = framed(got, 0xc0U, 0x08U);
	if (!hook || !power || !c4 || !c0) {
		std::vector<std::uint8_t> const head(got.begin(), got.begin() + std::min<std::size_t>(got.size(), 96U));
		std::cerr << "query block incomplete hook=" << hook << " power=" << power << " c4=" << c4 << " c0=" << c0
				  << " n=" << got.size() << " head=" << hex_dump(head) << "\n";
		return false;
	}
	return true;
}

bool config_frame_keeps_payload(std::vector<std::uint8_t> const &rom, bool embedded_opcode, std::uint8_t checksum,
	bool valid = true)
{
	millennium_am8048_core core;
	coinline::tp8048::pcd3349a_contract tp(core);
	tp.load_firmware_rom(rom.data(), rom.size());
	tp.reset();
	coinline::tp8048::tp_input_snapshot snap{};
	snap.keymatrix = 0x00080000U;
	tp.set_input_snapshot(snap);
	tp.step_tp_cycles_raw(400000ULL);
	tp.flush_boot_tx_queue();
	(void)drain_tx(tp);
	// The configuration can contain any octet, including apparent opcodes and
	// a checksum equal to its own header. None may restart the frame or alter relays.
	std::vector<std::uint8_t> frame(126U, 0U);
	frame[0] = 0xc0U;
	frame[1] = 0x7eU;
	frame[40] = 0xffU;
	if (embedded_opcode) {
		frame[41] = 0xc0U;
		frame[42] = 0x3dU;
	}
	unsigned sum = 0U;
	for (std::size_t i = 2; i < 124; ++i)
		sum += frame[i];
	frame[124] = static_cast<std::uint8_t>(checksum - sum);
	frame[125] = valid ? checksum : std::uint8_t(checksum ^ 1U);
	std::vector<std::uint8_t> got;
	for (std::uint8_t b : frame) {
		tp.receive_cp_byte(b);
		// Host polls advance the 8048 between CSI/O octets (~1e5 cycles at the CP clock).
		tp.step_tp_cycles_raw(100000ULL);
		auto const part = drain_tx(tp);
		got.insert(got.end(), part.begin(), part.end());
	}
	bool const ack = has_byte(got, 0x74U);
	bool const c4 = framed(got, 0xc4U, 0x04U);
	bool const c0 = framed(got, 0xc0U, 0x08U);
	if (!valid) {
		if (ack || c0) {
			std::cerr << "Corrupt configuration received an acknowledgement\n";
			return false;
		}
		return true;
	}
	if (!ack || !c4 || !c0 || tp.relay_cp_seized()) {
		std::vector<std::uint8_t> const head(got.begin(), got.begin() + std::min<std::size_t>(got.size(), 64U));
		std::cerr << "config frame incomplete ack=" << ack << " c4=" << c4 << " c0=" << c0
				  << " n=" << got.size() << " head=" << hex_dump(head) << "\n";
		return false;
	}
	return true;
}

bool handset_and_keypad(std::vector<std::uint8_t> const &rom)
{
	millennium_am8048_core core;
	coinline::tp8048::pcd3349a_contract tp(core);
	tp.load_firmware_rom(rom.data(), rom.size());
	tp.reset();
	coinline::tp8048::tp_input_snapshot snap{};
	snap.linectrl = 0x00U;
	tp.set_input_snapshot(snap);
	tp.step_tp_cycles_raw(400000ULL);
	tp.flush_boot_tx_queue();
	(void)drain_tx(tp);
	// Close the boot-ACK window so later OUTL bytes are kept.
	tp.receive_cp_byte(0x31U);
	(void)drain_tx(tp);

	constexpr std::uint32_t k_handset = 0x00080000U;
	snap.keymatrix = k_handset;
	tp.set_input_snapshot(snap);
	tp.step_tp_cycles_raw(200000ULL);
	tp.service_hook_timer_tail();
	auto got = drain_tx(tp);
	if (!has_byte(got, 0x62U) || !has_byte(got, 0x6EU)) {
		std::cerr << "handset off-hook missing 62/6E head=" << hex_dump(got) << "\n";
		return false;
	}

	struct keypad_case {
		std::uint32_t mask;
		std::uint8_t opcode;
	};
	constexpr keypad_case keys[] = {
		{0x00000800U, 0x38U},
		{0x00020000U, 0x54U}, {0x00010000U, 0x56U},
		{0x00040000U, 0x5AU}, {0x00001000U, 0x58U},
		{0x00100000U, 0x40U}, {0x00200000U, 0x42U}, {0x00400000U, 0x44U},
		{0x00800000U, 0x46U}, {0x01000000U, 0x48U},
	};
	for (auto const &key : keys) {
		snap.keymatrix = k_handset | key.mask;
		tp.set_input_snapshot(snap);
		tp.step_tp_cycles_raw(200000ULL);
		tp.drain_tx_ring();
		got = drain_tx(tp);
		if (!has_byte(got, key.opcode)) {
			std::cerr << "keypad expected 0x" << std::hex << unsigned(key.opcode)
					<< " received " << hex_dump(got) << "\n";
			return false;
		}
		snap.keymatrix = k_handset;
		tp.set_input_snapshot(snap);
		tp.step_tp_cycles_raw(200000ULL);
		(void)drain_tx(tp);
	}
	return true;
}

} // namespace

int main()
{
	if (!std::filesystem::exists(controller_rom)) {
		std::cerr << "skip: assemble the telephony controller model to run this test\n";
		return 77;
	}
	std::vector<std::uint8_t> rom;
	if (!load_rom(rom))
		return 1;
	if (!boot_ack_on_tx(true, 0x70U, rom))
		return 2;
	if (!boot_ack_on_tx(false, 0x72U, rom))
		return 3;
	if (!query_block(rom))
		return 4;
	if (!config_frame_keeps_payload(rom, false, 0xffU) ||
		!config_frame_keeps_payload(rom, false, 0xc0U) ||
		!config_frame_keeps_payload(rom, true, 0xc0U) ||
		!config_frame_keeps_payload(rom, false, 0xc0U, false))
		return 5;
	if (!handset_and_keypad(rom))
		return 6;
	std::cout << "pcd3349a_boot_query_block ok\n";
	return 0;
}
