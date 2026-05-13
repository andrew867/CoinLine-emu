// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_board_profile.h"

#include <cctype>
#include <vector>

namespace {

bool parse_uint_after(std::string const &text, char const *key, unsigned &out)
{
	auto p = text.find(key);
	if (p == std::string::npos)
		return false;
	auto colon = text.find(':', p);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
		++i;
	unsigned v = 0;
	while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
		v = v * 10U + unsigned(text[i] - '0');
		++i;
	}
	out = v;
	return true;
}

bool parse_wait_int(std::string const &text, char const *key, int &out)
{
	auto p = text.find(key);
	if (p == std::string::npos)
		return false;
	auto colon = text.find(':', p);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])))
		++i;
	int v = 0;
	bool neg = false;
	if (i < text.size() && text[i] == '-') {
		neg = true;
		++i;
	}
	if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i])))
		return false;
	while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
		v = v * 10 + int(text[i] - '0');
		++i;
	}
	out = neg ? -v : v;
	return true;
}

} // namespace

bool millennium_board_parse_z180_profile(std::string const &board_json, millennium_z180_board_config &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_z180_board_config{};
	auto z = board_json.find("\"z180\"");
	if (z == std::string::npos) {
		// Leave defaults (clock_hz already set in out = {}).
		return true;
	}
	auto zobj = board_json.find('{', z);
	if (zobj == std::string::npos)
		return false;
	auto zend = board_json.find("\"modem\"", zobj);
	std::string const zsec =
		(zend != std::string::npos && zend > zobj) ? board_json.substr(zobj, zend - zobj) : board_json.substr(zobj);

	unsigned clk = 0;
	if (parse_uint_after(zsec, "\"clock_hz\"", clk) && clk > 0)
		out.clock_hz = clk;

	auto ws = zsec.find("\"wait_states\"");
	if (ws != std::string::npos) {
		auto wopen = zsec.find('{', ws);
		auto wclose = zsec.find('}', wopen == std::string::npos ? ws : wopen);
		if (wopen != std::string::npos && wclose != std::string::npos && wclose > wopen) {
			std::string const wtxt = zsec.substr(wopen, wclose - wopen + 1);
			int v = -1;
			if (parse_wait_int(wtxt, "\"rom\"", v))
				out.wait_rom = v;
			v = -1;
			if (parse_wait_int(wtxt, "\"ram\"", v))
				out.wait_ram = v;
			v = -1;
			if (parse_wait_int(wtxt, "\"io\"", v))
				out.wait_io = v;
		}
	}

	return true;
}

bool millennium_board_parse_display_profile(std::string const &board_json, millennium_display_profile &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_display_profile{};
	auto d = board_json.find("\"display\"");
	if (d == std::string::npos)
		return true;
	auto dopen = board_json.find('{', d);
	if (dopen == std::string::npos)
		return false;
	auto dend = board_json.find("\"keypad\"", dopen);
	std::string const dsec =
		(dend != std::string::npos && dend > dopen) ? board_json.substr(dopen, dend - dopen) : board_json.substr(dopen);

	auto vpos = dsec.find("\"variant\"");
	if (vpos != std::string::npos) {
		auto q1 = dsec.find('"', dsec.find(':', vpos) + 1);
		if (q1 != std::string::npos) {
			auto q2 = dsec.find('"', q1 + 1);
			if (q2 != std::string::npos && q2 > q1)
				out.variant = dsec.substr(q1 + 1, q2 - q1 - 1);
		}
	}

	unsigned rows = 0, cols = 0;
	if (parse_uint_after(dsec, "\"rows\"", rows) && rows > 0 && rows <= 64)
		out.rows = int(rows);
	if (parse_uint_after(dsec, "\"columns\"", cols) && cols > 0 && cols <= 256)
		out.columns = int(cols);

	unsigned bch = 0, bcl = 0, bcu = 0;
	if (parse_uint_after(dsec, "\"busy_cycles_char\"", bch) && bch > 0)
		out.busy_cycles_char = int(bch);
	if (parse_uint_after(dsec, "\"busy_cycles_clear\"", bcl) && bcl > 0)
		out.busy_cycles_clear = int(bcl);
	if (parse_uint_after(dsec, "\"busy_cycles_cursor\"", bcu) && bcu > 0)
		out.busy_cycles_cursor = int(bcu);

	auto const idlek = dsec.find("\"idle_fixture\"");
	if (idlek != std::string::npos) {
		auto const colon = dsec.find(':', idlek);
		if (colon != std::string::npos) {
			auto q1 = dsec.find('"', colon + 1);
			if (q1 != std::string::npos) {
				auto q2 = dsec.find('"', q1 + 1);
				if (q2 != std::string::npos && q2 > q1)
					out.idle_fixture_relpath = dsec.substr(q1 + 1, q2 - q1 - 1);
			}
		}
	}
	if (out.idle_fixture_relpath.empty()) {
		if (out.variant.find("11") != std::string::npos)
			out.idle_fixture_relpath = "fixtures/display/vfd-11line-ad.json";
		else
			out.idle_fixture_relpath = "fixtures/display/vfd-2line-idle.json";
	}

	return true;
}

namespace {

bool slice_object_after_key(std::string const &board_json, char const *key, char const *next_key, std::string &out_sec)
{
	auto d = board_json.find(key);
	if (d == std::string::npos)
		return false;
	auto dopen = board_json.find('{', d);
	if (dopen == std::string::npos)
		return false;
	auto dend = board_json.find(next_key, dopen);
	out_sec = (dend != std::string::npos && dend > dopen) ? board_json.substr(dopen, dend - dopen) : board_json.substr(dopen);
	return true;
}

} // namespace

namespace {

bool count_json_string_array_elements(std::string const &sec, char const *key, unsigned &out_count)
{
	out_count = 0;
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const lb = sec.find('[', kpos);
	if (lb == std::string::npos)
		return false;
	auto const rb = sec.find(']', lb);
	if (rb == std::string::npos || rb <= lb)
		return false;
	std::string const inner = sec.substr(lb + 1, rb - lb - 1);
	unsigned c = 0;
	for (std::size_t i = 0; i < inner.size();) {
		while (i < inner.size()
			&& (inner[i] == ' ' || inner[i] == '\t' || inner[i] == '\n' || inner[i] == '\r' || inner[i] == ','))
			++i;
		if (i >= inner.size())
			break;
		if (inner[i] != '"')
			return false;
		++i;
		while (i < inner.size() && inner[i] != '"')
			++i;
		if (i >= inner.size())
			return false;
		++i;
		++c;
	}
	out_count = c;
	return true;
}

bool parse_json_quoted_string(std::string const &sec, char const *key, std::string &out)
{
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = sec.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	auto q1 = sec.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto q2 = sec.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return false;
	out = sec.substr(q1 + 1, q2 - q1 - 1);
	return true;
}

bool parse_json_int_array(std::string const &sec, char const *key, std::vector<int> &out)
{
	out.clear();
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const lb = sec.find('[', kpos);
	if (lb == std::string::npos)
		return false;
	auto const rb = sec.find(']', lb);
	if (rb == std::string::npos || rb <= lb)
		return false;
	std::string const inner = sec.substr(lb + 1, rb - lb - 1);
	for (std::size_t i = 0; i < inner.size();) {
		while (i < inner.size() && (inner[i] == ' ' || inner[i] == '\t' || inner[i] == ','))
			++i;
		if (i >= inner.size())
			break;
		int sign = 1;
		if (inner[i] == '-') {
			sign = -1;
			++i;
		}
		if (i >= inner.size() || !std::isdigit(static_cast<unsigned char>(inner[i])))
			break;
		int v = 0;
		while (i < inner.size() && std::isdigit(static_cast<unsigned char>(inner[i]))) {
			v = v * 10 + int(inner[i] - '0');
			++i;
		}
		out.push_back(sign * v);
	}
	return !out.empty();
}

} // namespace

bool millennium_board_parse_keypad_profile(std::string const &board_json, millennium_keypad_board_config &out,
	std::string &error_out)
{
	out = millennium_keypad_board_config{};
	std::string dsec;
	if (!slice_object_after_key(board_json, "\"keypad\"", "\"card_reader\"", dsec))
		return true;

	unsigned v = 0;
	if (parse_uint_after(dsec, "\"debounce_cycles\"", v) && v > 0 && v < 1000000U)
		out.debounce_cycles = int(v);
	v = 0;
	if (parse_uint_after(dsec, "\"scan_min_total_reads\"", v) && v > 0 && v < 1000000U)
		out.scan_min_total_reads = int(v);
	v = 0;
	if (parse_uint_after(dsec, "\"scan_min_pb_deltas\"", v) && v > 0 && v < 1000000U)
		out.scan_min_pb_deltas = int(v);

	unsigned qk = 0;
	if (count_json_string_array_elements(dsec, "\"quick_access_keys\"", qk)) {
		if (qk != 5U && qk != 10U) {
			error_out = "keypad.quick_access_keys must contain 5 or 10 entries";
			return false;
		}
		out.quick_access_key_count = qk;
	}

	std::string t21;
	if (parse_json_quoted_string(dsec, "\"terminal_21_profile_id\"", t21)) {
		for (char &ch : t21)
			ch = char(std::tolower(static_cast<unsigned char>(ch)));
		if (t21 == "repdial_5") {
			out.terminal_21_profile = millennium_terminal21_user_io_profile::repdial_5;
			out.terminal_21_profile_explicit = true;
		} else if (t21 == "repdial_10") {
			out.terminal_21_profile = millennium_terminal21_user_io_profile::repdial_10;
			out.terminal_21_profile_explicit = true;
		} else if (t21 == "vfd_11line_softkeys") {
			out.terminal_21_profile = millennium_terminal21_user_io_profile::vfd_11line_softkeys;
			out.terminal_21_profile_explicit = true;
		}
	}

	return true;
}

bool millennium_board_parse_security_profile(std::string const &board_json, millennium_security_board_config &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_security_board_config{};
	std::string dsec;
	if (!slice_object_after_key(board_json, "\"security\"", "\"memory\"", dsec))
		return true;

	unsigned sv = 0;
	if (parse_uint_after(dsec, "\"debounce_cycles\"", sv) && sv > 0 && sv < 1000000U)
		out.debounce_cycles = int(sv);

	return true;
}

bool millennium_board_parse_coin_profile(std::string const &board_json, millennium_coin_board_config &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_coin_board_config{};
	std::string dsec;
	if (!slice_object_after_key(board_json, "\"coin\"", "\"alerter\"", dsec))
		return true;

	std::string vt;
	if (parse_json_quoted_string(dsec, "\"validator_type\"", vt))
		out.validator_type = vt;

	std::vector<int> denoms;
	if (parse_json_int_array(dsec, "\"denominations\"", denoms))
		out.denominations_cents = std::move(denoms);

	unsigned pw = 0, ig = 0;
	if (parse_uint_after(dsec, "\"pulse_width_us\"", pw) && pw > 0 && pw < 1000000U)
		out.pulse_width_us = pw;
	if (parse_uint_after(dsec, "\"inter_pulse_gap_us\"", ig) && ig > 0 && ig < 1000000U)
		out.inter_pulse_gap_us = ig;

	return true;
}

bool millennium_board_parse_alerter_profile(std::string const &board_json, millennium_alerter_board_config &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_alerter_board_config{};
	std::string dsec;
	if (!slice_object_after_key(board_json, "\"alerter\"", "\"security\"", dsec))
		return true;

	unsigned sr = 0;
	if (parse_uint_after(dsec, "\"sample_rate_hz\"", sr) && sr >= 4000 && sr <= 48000)
		out.sample_rate_hz = sr;

	unsigned cpu_hz = 0;
	if (parse_uint_after(dsec, "\"cpu_clock_hz\"", cpu_hz) && cpu_hz >= 1000000U && cpu_hz <= 200000000U)
		out.cpu_clock_hz = static_cast<double>(cpu_hz);

	return true;
}

static bool parse_json_hex_u32_field(std::string const &sec, char const *key, std::uint32_t &out)
{
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = sec.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	auto q1 = sec.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto q2 = sec.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return false;
	std::string const hexstr = sec.substr(q1 + 1, q2 - q1 - 1);
	std::size_t i = 0;
	if (hexstr.size() >= 2 && hexstr[0] == '0' && (hexstr[1] == 'x' || hexstr[1] == 'X'))
		i = 2;
	if (i >= hexstr.size())
		return false;
	std::uint64_t v = 0;
	for (; i < hexstr.size(); ++i) {
		char c = hexstr[i];
		int d = -1;
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else
			return false;
		v = (v << 4) | std::uint64_t(d);
		if (v > 0xffffffffULL)
			return false;
	}
	out = std::uint32_t(v);
	return true;
}

bool millennium_board_parse_memory_layout(std::string const &board_json, millennium_memory_layout_config &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_memory_layout_config{};
	std::string dsec;
	if (!slice_object_after_key(board_json, "\"memory\"", "\"z180\"", dsec))
		return false;

	std::uint32_t v = 0;
	if (!parse_json_hex_u32_field(dsec, "\"nvram_base\"", v)) {
		error_out = "memory.nvram_base missing or invalid";
		return false;
	}
	out.nvram_base = v;

	unsigned sz = 0;
	if (!parse_uint_after(dsec, "\"nvram_size\"", sz) || sz == 0) {
		error_out = "memory.nvram_size missing or invalid";
		return false;
	}
	out.nvram_size = sz;

	if (!parse_json_hex_u32_field(dsec, "\"table_storage_base\"", v)) {
		error_out = "memory.table_storage_base missing or invalid";
		return false;
	}
	out.table_storage_base = v;

	sz = 0;
	if (!parse_uint_after(dsec, "\"table_storage_size\"", sz) || sz == 0) {
		error_out = "memory.table_storage_size missing or invalid";
		return false;
	}
	out.table_storage_size = sz;

	if (!parse_json_hex_u32_field(dsec, "\"dla_stage_base\"", v)) {
		error_out = "memory.dla_stage_base missing or invalid";
		return false;
	}
	out.dla_stage_base = v;

	sz = 0;
	if (!parse_uint_after(dsec, "\"dla_stage_size\"", sz) || sz == 0) {
		error_out = "memory.dla_stage_size missing or invalid";
		return false;
	}
	out.dla_stage_size = sz;

	return true;
}

static bool parse_json_bool_field(std::string const &sec, char const *key, bool &out)
{
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = sec.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < sec.size() && (sec[i] == ' ' || sec[i] == '\t' || sec[i] == '\n' || sec[i] == '\r'))
		++i;
	if (i + 4 <= sec.size() && sec.compare(i, 4, "true") == 0) {
		out = true;
		return true;
	}
	if (i + 5 <= sec.size() && sec.compare(i, 5, "false") == 0) {
		out = false;
		return true;
	}
	return false;
}

static bool extract_braced_object_after(std::string const &text, char const *key, std::string &out_obj)
{
	auto const kpos = text.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const o = text.find('{', kpos);
	if (o == std::string::npos)
		return false;
	int depth = 0;
	for (std::size_t i = o; i < text.size(); ++i) {
		if (text[i] == '{')
			depth++;
		else if (text[i] == '}') {
			depth--;
			if (depth == 0) {
				out_obj = text.substr(o, i - o + 1);
				return true;
			}
		}
	}
	return false;
}

bool millennium_board_parse_user_io_section(std::string const &board_json, millennium_user_io_board_config &out,
	std::string &error_out)
{
	(void)error_out;
	out = millennium_user_io_board_config{};
	std::string usec;
	if (!extract_braced_object_after(board_json, "\"user_io\"", usec))
		return true;

	std::string otsec;
	if (extract_braced_object_after(usec, "\"overlay_traits\"", otsec)) {
		bool v = false;
		if (parse_json_bool_field(otsec, "\"adsi_active\"", v))
			out.overlay.adsi_active = v;
		if (parse_json_bool_field(otsec, "\"proton_active\"", v))
			out.overlay.proton_active = v;
		if (parse_json_bool_field(otsec, "\"mondex_active\"", v))
			out.overlay.mondex_active = v;
		if (parse_json_bool_field(otsec, "\"git_ui_active\"", v))
			out.overlay.git_ui_active = v;
		if (parse_json_bool_field(otsec, "\"data_jack_manual_keypad_active\"", v))
			out.overlay.data_jack_manual_keypad_active = v;
	}

	std::string polsec;
	if (extract_braced_object_after(usec, "\"policy\"", polsec)) {
		bool v = false;
		if (parse_json_bool_field(polsec, "\"user_if_active\"", v))
			out.policy.user_if_active = v;
		if (parse_json_bool_field(polsec, "\"protection_blocks_user_if_soft_actions\"", v))
			out.policy.protection_blocks_user_if_soft_actions = v;
		if (parse_json_bool_field(polsec, "\"adsi_runtime_session_active\"", v))
			out.policy.adsi_runtime_session_active = v;
		if (parse_json_bool_field(polsec, "\"overlay_blocks_language_next_call\"", v))
			out.policy.overlay_blocks_language_next_call = v;
		if (parse_json_bool_field(polsec, "\"proton_ui_blocks_language_next_call\"", v))
			out.policy.proton_ui_blocks_language_next_call = v;
		if (parse_json_bool_field(polsec, "\"mondex_local_ui_blocks_language_next_call\"", v))
			out.policy.mondex_local_ui_blocks_language_next_call = v;
		if (parse_json_bool_field(polsec, "\"git_ui_blocks_language_next_call\"", v))
			out.policy.git_ui_blocks_language_next_call = v;
		if (parse_json_bool_field(polsec, "\"protection_blocks_dial_pad\"", v))
			out.policy.protection_blocks_dial_pad = v;
		if (parse_json_bool_field(polsec, "\"cp_absorb_blocked_user_if_opcodes\"", v))
			out.policy.cp_absorb_blocked_user_if_opcodes = v;
	}
	return true;
}

void millennium_board_resolve_terminal21_profile(millennium_display_profile const &display, millennium_keypad_board_config &keypad)
{
	if (keypad.terminal_21_profile_explicit)
		return;
	bool vfd11 = display.rows >= 11;
	std::string v = display.variant;
	for (char &ch : v)
		ch = char(std::tolower(static_cast<unsigned char>(ch)));
	if (v.find("11line") != std::string::npos)
		vfd11 = true;
	if (vfd11) {
		keypad.terminal_21_profile = millennium_terminal21_user_io_profile::vfd_11line_softkeys;
		return;
	}
	if (keypad.quick_access_key_count > 0U && keypad.quick_access_key_count <= 5U)
		keypad.terminal_21_profile = millennium_terminal21_user_io_profile::repdial_5;
	else
		keypad.terminal_21_profile = millennium_terminal21_user_io_profile::repdial_10;
}
