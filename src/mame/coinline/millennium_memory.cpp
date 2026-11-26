// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_memory.h"

#include "millennium_state.h"

void millennium_map_program(address_map &map, millennium_state &state)
{
	// Low 768 KiB: program flash, with earlier-board writable overlay handling.
	map(0x00000, 0xbffff)
		.lrw8(
			NAME([&state](offs_t offset) -> u8 { return state.phys_low_r(offset); }),
			NAME([&state](offs_t offset, u8 data) { state.phys_low_w(offset, data); }));
	// Fixed common SRAM and selectable upper window share one physical device.
	// Both CPU windows use the same decoder in phys_ram_r/w.
	map(0xc0000, 0xdffff)
		.lrw8(
			NAME([&state](offs_t offset) -> u8 { return state.phys_ram_r(offs_t(0xc0000U + offset)); }),
			NAME([&state](offs_t offset, u8 data) { state.phys_ram_w(offs_t(0xc0000U + offset), data); }));
	map(0xe0000, 0xfffff)
		.lrw8(
			NAME([&state](offs_t offset) -> u8 { return state.phys_ram_r(offs_t(0xe0000U + offset)); }),
			NAME([&state](offs_t offset, u8 data) { state.phys_ram_w(offs_t(0xe0000U + offset), data); }));

	millennium_memory_layout_config const &ly = state.memory_layout();
	// Extended regions follow the board profile's physical address map.
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
