// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_memory.h"

#include "millennium_state.h"

void millennium_map_program(address_map &map, millennium_state &state)
{
	// 768 KiB low: flash image for reads; writable overlay for stores (Z180 MMU maps logical
	// stack/data to physical addresses below 0xC0000 — see DLAPP.MAP bank vs SRAM split).
	map(0x00000, 0xbffff)
		.lrw8(
			NAME([&state](offs_t offset) -> u8 { return state.phys_low_r(offset); }),
			NAME([&state](offs_t offset, u8 data) { state.phys_low_w(offset, data); }));
	// SRAM (512 KiB device): decode/banking from **PIO_PORT_H** latch is centralized in
	// **millennium_mach_decode_phys_ram** (`millennium_mach_pio.cpp`). Two CPU windows call **phys_ram_r/w**.
	map(0xc0000, 0xdffff)
		.lrw8(
			NAME([&state](offs_t offset) -> u8 { return state.phys_ram_r(offs_t(0xc0000U + offset)); }),
			NAME([&state](offs_t offset, u8 data) { state.phys_ram_w(offs_t(0xc0000U + offset), data); }));
	map(0xe0000, 0xfffff)
		.lrw8(
			NAME([&state](offs_t offset) -> u8 { return state.phys_ram_r(offs_t(0xe0000U + offset)); }),
			NAME([&state](offs_t offset, u8 data) { state.phys_ram_w(offs_t(0xe0000U + offset), data); }));

	millennium_memory_layout_config const &ly = state.memory_layout();
	if (ly.nvram_size != 0U)
		map(ly.nvram_base, ly.nvram_base + ly.nvram_size - 1)
			.lrw8(
				NAME([&state](offs_t offset) -> u8 { return state.storage_nvram_r(offset); }),
				NAME([&state](offs_t offset, u8 data) { state.storage_nvram_w(offset, data); }));
	if (ly.table_storage_size != 0U)
		map(ly.table_storage_base, ly.table_storage_base + ly.table_storage_size - 1)
			.lrw8(
				NAME([&state](offs_t offset) -> u8 { return state.storage_table_r(offset); }),
				NAME([&state](offs_t offset, u8 data) { state.storage_table_w(offset, data); }));
	if (ly.dla_stage_size != 0U)
		map(ly.dla_stage_base, ly.dla_stage_base + ly.dla_stage_size - 1)
			.lrw8(
				NAME([&state](offs_t offset) -> u8 { return state.storage_dla_r(offset); }),
				NAME([&state](offs_t offset, u8 data) { state.storage_dla_w(offset, data); }));
}
