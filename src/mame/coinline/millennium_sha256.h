// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

void millennium_sha256_reset();
void millennium_sha256_update(std::uint8_t const *data, std::size_t len);
void millennium_sha256_finish(std::uint8_t out_digest[32]);
std::string millennium_sha256_hex(std::uint8_t const *data, std::size_t len);
