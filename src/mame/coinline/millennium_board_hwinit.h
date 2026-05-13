// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>

/// Logical port images after Millennium board PIO bring-up (8255 ports A–C, port G, MACH PIO H/D/E/F).
/// \param new_hardware_revision_1 When true, PIO port A image is \c 0xBF; when false, \c 0xFF.
void millennium_hwinit_apply_pio_port_initialize(bool new_hardware_revision_1,
	std::array<std::uint8_t, 4> &pio_8255_shadow,
	std::uint8_t &pio_port_g,
	std::array<std::uint8_t, 4> &mach_pio_shadow);

/// External UART @ \c 0xE0 shadow state after coin-validator bring-up (600 baud, 8N1 programmed):
/// TL16C550-class (16550-style register map); shadow reflects post-init DLAB-off view,
/// \c MCR=0, \c IER RX interrupt enabled. Divisor \c 0x280 (\c DLL=\c 0x80, \c DLM=\c 0x02) is mirrored in dedicated latch bytes alongside this snapshot.
void millennium_hwinit_apply_coin_validator_tl16c550(std::array<std::uint8_t, 8> &ext_uart_shadow);
