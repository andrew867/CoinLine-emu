// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>

/// Hardware-visible cash vault / cover inputs share MACH port \c H reads with the upper-RAM latch (see
/// \c millennium_mach_pio_combine_port_h_read). Firmware **collection** logic (status records, escrow,
/// datalog records) runs entirely in RAM/NVRAM with NMI masking — there is **no** separate cash-box GPIO device
/// or security IC beyond these sense bits and normal coin/NVRAM paths. Higher-level state for tests lives in
/// \c cashbox_collection_model.

namespace millennium_cashbox_hw {

inline constexpr std::uint8_t k_mach_port_h_vault_status_mask = 0x0CU; ///< Bits merged on read (cover / removed).

} // namespace millennium_cashbox_hw
