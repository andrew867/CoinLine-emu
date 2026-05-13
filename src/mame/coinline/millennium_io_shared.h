// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <string>

// Parse fixtures/board/io-port-map.json (subset): unknown_default and per-port status.
bool millennium_io_parse_port_map(std::string const &json_text, std::uint8_t &unknown_default_out,
	std::array<bool, 256> &known_or_suspected_out, std::string &error_out);

std::string millennium_format_unknown_port_json(std::string const &ts_rfc3339, std::uint64_t cycle,
	std::uint16_t pc, std::uint16_t port_full, bool is_write, std::uint8_t value, char const *source_symbol,
	char const *note);

/// True for common affirmative env/INI tokens (`1`, `true`, `yes`, `on`), case-insensitive.
bool coinline_env_value_is_truthy(char const *value);

/// True for common negative tokens (`0`, `false`, `no`, `off`), case-insensitive.
bool coinline_env_value_is_falsey(char const *value);

/// **Default-on** policy: unset or empty → `true`; explicitly falsey → `false`; any other non-empty string → `true`.
bool coinline_env_default_true_unless_falsey(char const *value);
