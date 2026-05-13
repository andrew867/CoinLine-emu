// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "emu.h"

class millennium_state;

/// I/O map (Z180 16-bit); see `fixtures/board/voiceware-command-map.json` for 0x40/0x42/0x61 voiceware wiring.
void millennium_configure_io_map(address_map &map, millennium_state &state);
