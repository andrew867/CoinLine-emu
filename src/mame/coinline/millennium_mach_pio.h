// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

/// Combined read for MACH PIO port H (I/O **0xC0**).
/// Writes store upper-RAM bank/latch bits in the shadow; reads clear vault/cover bits **2–3** for nominal polls
/// and OR in smart-card lines on bits **0–1** (overlap with latch bits — see implementation).
/// Firmware cash-box **collection** (records, escrow, NMI-guarded updates) is software/NVRAM — not a separate chip.
/// \p smartcard_lines_low2 uses only bits 0–1.
std::uint8_t millennium_mach_pio_combine_port_h_read(std::uint8_t shadow, std::uint8_t smartcard_lines_low2);

/// Physical memory routing modeled after CPLD/PAL glue driven by MACH port \c H (\c 0xC0) latch:
/// bits **4–5** select which **128 KiB** slice of the **512 KiB** SRAM appears at **0xC0000–0xDFFFF**;
/// when bits **0–2** are all set, **0xE0000–0xFFFFF** maps the **next** 128 KiB of that slice (otherwise
/// that window tracks linear flash). Firmware \c PAGERAM logical **pages** (e.g. visual-prompt tables)
/// are software partitioning inside that SRAM — the same I/O latch selects physical silicon.
enum class millennium_mach_phys_ram_route : std::uint8_t {
	below_sram_windows, ///< below **0xC0000** — not SRAM decode (CPU map uses overlay/flash elsewhere)
	sram_chip,          ///< **chip_byte_index** is valid into the backing SRAM vector
	upper_flash,        ///< **0xE0000–0xFFFFF** sees flash (glue did not select SRAM or window overflow)
	unmapped_ff,        ///< decode fault / out-of-range lower window (should not occur at nominal capacity)
};

struct millennium_mach_phys_ram_decode {
	millennium_mach_phys_ram_route route{};
	std::size_t chip_byte_index{};
};

millennium_mach_phys_ram_decode millennium_mach_decode_phys_ram(std::uint8_t mach_port_h_shadow,
	std::uint32_t phys_addr_20, std::size_t sram_capacity_bytes);
