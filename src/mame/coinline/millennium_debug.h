// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "millennium_z180_snapshot.h"

std::string millennium_boot_trace_timestamp_utc();

void millennium_boot_trace_append_line(std::filesystem::path const &path, std::string const &json_object);

/// Single-line JSON for COINLINE_IO_TRACE (append-only JSONL).
std::string millennium_format_io_trace_line(std::uint64_t cycle, std::uint16_t port, char rw, std::uint8_t data,
	char const *tag);

/// Rich JSONL for debug runs: PC, SP, milestone, optional decoded device.
std::string millennium_format_io_trace_line_v2(std::uint64_t cycle, std::uint16_t port, char rw, std::uint8_t data,
	char const *tag, std::uint16_t pc, std::uint16_t sp, char const *milestone);

std::string millennium_format_memory_trace_line(std::uint64_t cycle, std::uint32_t phys_addr, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, char const *milestone);

/// JSONL for emulated NVRAM region access (logical EEPROM / battery-backed array), not MAME's .nv persistence file I/O.
std::string millennium_format_nvram_storage_trace_line(std::uint64_t cycle, std::uint32_t region_offset,
	std::uint32_t phys_addr, char rw, std::uint8_t data, std::uint16_t pc, std::uint16_t sp, char const *milestone);

/// JSONL for on-board 93LC66 Microwire bit-bang model (logical word reads/writes and control ops).
std::string millennium_format_microwire_trace_line(std::uint64_t cycle, char const *op, std::uint16_t word_addr,
	std::uint16_t word_be, std::uint16_t pc, std::uint16_t sp, char const *milestone);

std::string millennium_format_cpu_trace_line(std::uint64_t cycle, std::uint16_t pc, std::uint8_t op0, std::uint8_t op1,
	std::uint8_t op2, std::uint16_t sp, bool iff1, char const *milestone);

std::string millennium_format_z180_reg_trace_line(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp, bool iff1,
	bool iff2, millennium_z180_snapshot const &s, char const *milestone);

std::string millennium_format_stack_trace_line(std::uint64_t cycle, std::uint32_t phys_addr, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, std::uint32_t sp_phys20, char const *region_tag, char const *milestone);

std::string millennium_format_ram_init_trace_line(std::uint64_t cycle, std::uint32_t phys_addr, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, char const *region_tag, char const *milestone);

std::string millennium_format_mmu_translation_trace_line(std::uint64_t cycle, std::uint16_t pc_logical,
	std::uint16_t sp_logical, std::uint8_t cbr, std::uint8_t bbr, std::uint8_t cbar, std::uint32_t pc_phys20,
	std::uint32_t sp_phys20, char const *milestone);

std::string millennium_boot_trace_m0(std::string const &ts, std::string const &sha256_hex,
	std::uint64_t size_bytes);

std::string millennium_boot_trace_m1(std::string const &ts, std::uint16_t pc, std::string const &opcode_hex);

std::string millennium_boot_trace_m2(std::string const &ts, std::uint16_t pc);

std::string millennium_boot_trace_m3(std::string const &ts, std::uint64_t ram_writes, std::uint16_t sp,
	char const *m3_trigger);

std::string millennium_boot_trace_m5(std::string const &ts, std::string const &device, std::string const &pc_hex);

/// First firmware write to voiceware phrase port `VOICE_SYNTHESIS_CODE_ADDR` (0x0061). Not VFD (M6).
std::string millennium_boot_trace_m5v(std::string const &ts, std::string const &pc_hex, std::string const &phrase_hex);

/// Phrase command reached the uPD7759 start/strobe path (port + start pulse).
std::string millennium_boot_trace_m5a(std::string const &ts, std::string const &pc_hex, std::string const &phrase_hex);

/// uPD7759 returned to idle (phrase segment completed or never started) after having been active.
std::string millennium_boot_trace_m5c(std::string const &ts, std::string const &pc_hex);

std::string millennium_boot_trace_m6(std::string const &ts, std::string const &vfd_summary);

std::string millennium_boot_trace_m7(std::string const &ts, std::uint64_t keypad_scan_count);

std::string millennium_boot_trace_m8(std::string const &ts, std::string const &asci_summary, bool dcd, bool cts);

std::string millennium_boot_trace_m9(std::string const &ts, std::string const &scheduler_pc_hex);

std::string millennium_boot_trace_m10(std::string const &ts, std::string const &vfd_summary);

/// JSONL object for event-level CPU/trace probes (`interrupt-events.jsonl`, `vector-events.jsonl`, `context-switch-events.jsonl`).
std::string millennium_format_vector_event_line(std::uint64_t cycle, char const *event, std::uint16_t pc,
	std::uint16_t sp, std::uint8_t op0, std::uint8_t op1, bool iff1, int im, char const *milestone);

/// Rich event schema for EI/DI/IM/RET* opcode probes (`ei-di-events.jsonl`).
std::string millennium_format_eidi_event_line(std::uint64_t cycle, std::uint16_t pc, std::uint8_t opcode0,
	std::uint8_t opcode1, char const *mnemonic, std::uint16_t sp, bool iff1_before, bool iff2_before,
	bool iff1_after, bool iff2_after, char const *nearby_symbol, char const *note, char const *milestone);

std::string millennium_format_voiceware_trace_line(std::uint64_t cycle, char rw, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, std::uint8_t hw_cntl_shadow, char const *milestone);
