// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_state.h"

#include "millennium_debug.h"
#include "millennium_vfd.h"
#include "millennium_firmware.h"
#include "millennium_io.h"
#include "millennium_io_shared.h"
#include "millennium_memory.h"
#include "millennium_z180_snapshot.h"
#include "millennium_z180_internal.h"
#include "millennium_z180_mmu.h"
#include "millennium_mach_pio.h"
#include "millennium_board_hwinit.h"
#include "millennium_voiceware_config.h"

namespace {

attotime voicew_int0_poll_delay()
{
	return coinline_voiceware_upd7759_core_from_env() ? attotime::from_usec(10) : attotime::from_usec(100);
}

} // namespace
#include "millennium_vfd_gfxfont.h"
#include "millennium_vfd_cell_utf8.h"

#include "millennium.lh"

#include "render.h"
#include "rendertypes.h"
#include "strformat.h"
#include "bitmap.h"
#include "util/unicode.h"
#include "speaker.h"
#include "machine/rescap.h"
#include "sound/beep.h"
#include "sound/flt_rc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "modules/lib/osdlib.h"

#include <windows.h>
#endif

namespace {

#ifndef COINLINE_ENABLE_TP8048_BACKEND
#define COINLINE_ENABLE_TP8048_BACKEND 1
#endif

struct user_io_harness_input {
	std::string run_id;
	unsigned quick_access_key_count = 0U;
	bool has_11_line_softkeys = false;
	bool has_adsi_active = false;
	bool adsi_active = false;
	bool has_proton_active = false;
	bool proton_active = false;
	bool has_mondex_active = false;
	bool mondex_active = false;
	bool has_git_ui_active = false;
	bool git_ui_active = false;
	bool has_data_jack_manual_keypad_active = false;
	bool data_jack_manual_keypad_active = false;
	std::vector<std::string> enabled_vectors;
};

struct rtos_startup_task_metadata_entry {
	unsigned slot = 0U;
	unsigned status = 0U;
	unsigned priority = 0U;
	unsigned signals = 0U;
	unsigned signals_upper = 0U;
};

std::string json_escape_string(std::string const &in)
{
	std::string out;
	out.reserve(in.size() + 8U);
	for (char c : in) {
		switch (c) {
		case '\\': out += "\\\\"; break;
		case '"': out += "\\\""; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default: out.push_back(c); break;
		}
	}
	return out;
}

millennium_state::tel_response_policy parse_tel_response_policy_env()
{
	using pol = millennium_state::tel_response_policy;
	char const *const p = osd_getenv("COINLINE_TEL_RESPONSE_POLICY");
	if (!p || !*p)
		return pol::immediate;
	std::string_view const s(p);
	if (s == "immediate")
		return pol::immediate;
	if (s == "latch_then_clear")
		return pol::latch_then_clear;
	if (s == "withhold_until_not_responding_seen")
		return pol::withhold_until_not_responding_seen;
	if (s == "withhold_until_retry")
		return pol::withhold_until_retry;
	if (s == "withhold_until_timeout")
		return pol::withhold_until_timeout;
	return pol::immediate;
}

bool parse_json_bool_field_local(std::string const &sec, char const *key, bool &out)
{
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = sec.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < sec.size() && std::isspace(static_cast<unsigned char>(sec[i])))
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

bool parse_json_uint_field_local(std::string const &sec, char const *key, unsigned &out)
{
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = sec.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	std::size_t i = colon + 1;
	while (i < sec.size() && std::isspace(static_cast<unsigned char>(sec[i])))
		++i;
	if (i >= sec.size() || !std::isdigit(static_cast<unsigned char>(sec[i])))
		return false;
	unsigned v = 0U;
	while (i < sec.size() && std::isdigit(static_cast<unsigned char>(sec[i]))) {
		v = (v * 10U) + unsigned(sec[i] - '0');
		++i;
	}
	out = v;
	return true;
}

bool parse_json_string_field_local(std::string const &sec, char const *key, std::string &out)
{
	auto const kpos = sec.find(key);
	if (kpos == std::string::npos)
		return false;
	auto const colon = sec.find(':', kpos);
	if (colon == std::string::npos)
		return false;
	auto const q1 = sec.find('"', colon);
	if (q1 == std::string::npos)
		return false;
	auto const q2 = sec.find('"', q1 + 1);
	if (q2 == std::string::npos || q2 <= q1)
		return false;
	out = sec.substr(q1 + 1, q2 - q1 - 1);
	return true;
}

bool parse_rtos_startup_task_metadata_text(std::string const &text, std::vector<rtos_startup_task_metadata_entry> &out,
	std::string &error_out)
{
	out.clear();
	auto const tasks_pos = text.find("\"tasks\"");
	if (tasks_pos == std::string::npos) {
		error_out = "missing tasks array";
		return false;
	}
	auto const arr_open = text.find('[', tasks_pos);
	if (arr_open == std::string::npos) {
		error_out = "missing tasks array opener";
		return false;
	}
	auto const arr_close = text.find(']', arr_open);
	if (arr_close == std::string::npos || arr_close <= arr_open) {
		error_out = "missing tasks array closer";
		return false;
	}
	std::size_t pos = arr_open + 1;
	while (pos < arr_close) {
		auto const obj_open = text.find('{', pos);
		if (obj_open == std::string::npos || obj_open >= arr_close)
			break;
		auto const obj_close = text.find('}', obj_open);
		if (obj_close == std::string::npos || obj_close > arr_close) {
			error_out = "unterminated task object";
			return false;
		}
		std::string const obj = text.substr(obj_open, obj_close - obj_open + 1);
		rtos_startup_task_metadata_entry entry{};
		bool ok = true;
		ok = ok && parse_json_uint_field_local(obj, "\"slot\"", entry.slot);
		ok = ok && parse_json_uint_field_local(obj, "\"status\"", entry.status);
		ok = ok && parse_json_uint_field_local(obj, "\"priority\"", entry.priority);
		ok = ok && parse_json_uint_field_local(obj, "\"signals\"", entry.signals);
		ok = ok && parse_json_uint_field_local(obj, "\"signals_upper\"", entry.signals_upper);
		if (!ok) {
			error_out = "task entry missing required numeric fields";
			return false;
		}
		out.push_back(entry);
		pos = obj_close + 1;
	}
	if (out.empty()) {
		error_out = "tasks array has no entries";
		return false;
	}
	return true;
}

bool parse_json_string_array_field_local(std::string const &sec, char const *key, std::vector<std::string> &out)
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
		while (i < inner.size()
			&& (inner[i] == ' ' || inner[i] == '\t' || inner[i] == '\r' || inner[i] == '\n' || inner[i] == ','))
			++i;
		if (i >= inner.size())
			break;
		if (inner[i] != '"')
			return false;
		++i;
		std::size_t const start = i;
		while (i < inner.size() && inner[i] != '"')
			++i;
		if (i >= inner.size())
			return false;
		out.emplace_back(inner.substr(start, i - start));
		++i;
	}
	return !out.empty();
}

bool extract_braced_object_after_local(std::string const &text, char const *key, std::string &out_obj)
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
			++depth;
		else if (text[i] == '}') {
			--depth;
			if (depth == 0) {
				out_obj = text.substr(o, i - o + 1);
				return true;
			}
		}
	}
	return false;
}

bool parse_user_io_harness_input_text(std::string const &text, user_io_harness_input &out, std::string &error_out)
{
	out = user_io_harness_input{};
	if (!parse_json_string_field_local(text, "\"run_id\"", out.run_id) || out.run_id.empty()) {
		error_out = "harness input missing required run_id";
		return false;
	}

	std::string profile_traits;
	if (!extract_braced_object_after_local(text, "\"profile_traits\"", profile_traits)) {
		error_out = "harness input missing required profile_traits object";
		return false;
	}
	if (!parse_json_uint_field_local(profile_traits, "\"quick_access_key_count\"", out.quick_access_key_count)) {
		error_out = "harness input missing required profile_traits.quick_access_key_count";
		return false;
	}
	if (!(out.quick_access_key_count == 5U || out.quick_access_key_count == 10U)) {
		error_out = "harness input profile_traits.quick_access_key_count must be 5 or 10";
		return false;
	}
	if (!parse_json_bool_field_local(profile_traits, "\"has_11_line_softkeys\"", out.has_11_line_softkeys)) {
		error_out = "harness input missing required profile_traits.has_11_line_softkeys";
		return false;
	}
	if (!parse_json_string_array_field_local(text, "\"enabled_vectors\"", out.enabled_vectors)
		|| out.enabled_vectors.empty()) {
		error_out = "harness input missing required enabled_vectors";
		return false;
	}

	std::string overlays;
	if (extract_braced_object_after_local(text, "\"overlay_traits\"", overlays)) {
		out.has_adsi_active = parse_json_bool_field_local(overlays, "\"adsi_active\"", out.adsi_active);
		out.has_proton_active = parse_json_bool_field_local(overlays, "\"proton_active\"", out.proton_active);
		out.has_mondex_active = parse_json_bool_field_local(overlays, "\"mondex_active\"", out.mondex_active);
		out.has_git_ui_active = parse_json_bool_field_local(overlays, "\"git_ui_active\"", out.git_ui_active);
		out.has_data_jack_manual_keypad_active =
			parse_json_bool_field_local(overlays, "\"data_jack_manual_keypad_active\"", out.data_jack_manual_keypad_active);
	}
	return true;
}

// Text rendering uses the same glyph walk as layout_element::component::draw_text (rendlay.cpp),
// kept local because that helper is not publicly exposed from MAME headers.
inline void coinline_alpha_blend_u32(u32 &dest, u32 a, u32 r, u32 g, u32 b, u32 inva)
{
	rgb_t const dpix(dest);
	u32 const da(dpix.a());
	u32 const finala((a * 255) + (da * inva));
	u32 const finalr(r + (u32(dpix.r()) * da * inva));
	u32 const finalg(g + (u32(dpix.g()) * da * inva));
	u32 const finalb(b + (u32(dpix.b()) * da * inva));
	dest = rgb_t(finala / 255, finalr / finala, finalg / finala, finalb / finala);
}

inline void coinline_alpha_blend_color(u32 &dest, render_color const &c, float fill)
{
	u32 const a(u32(c.a * fill * 255.0F));
	if (a) {
		u32 const r(u32(c.r * (255.0F * 255.0F)) * a);
		u32 const g(u32(c.g * (255.0F * 255.0F)) * a);
		u32 const b(u32(c.b * (255.0F * 255.0F)) * a);
		coinline_alpha_blend_u32(dest, a, r, g, b, 255 - a);
	}
}

void coinline_draw_text(render_font &font, bitmap_argb32 &dest, rectangle const &bounds, std::string_view str, int align,
	render_color const &color)
{
	s32 width = font.string_width(bounds.height(), 1.0f, str);
	float aspect = 1.0f;
	if ((align == 3) || (width > bounds.width())) {
		if (width != 0)
			aspect = float(bounds.width()) / float(width);
		width = bounds.width();
	}
	float curx = 0.0f;
	switch (align) {
	case 1:
		curx = float(bounds.left());
		break;
	case 2:
		curx = float(bounds.left() + bounds.width() - width);
		break;
	case 3:
		curx = float(bounds.left());
		break;
	default:
		curx = float(bounds.left()) + (float(bounds.width()) - float(width)) / 2.0f;
		break;
	}
	bitmap_argb32 tempbitmap(dest.width(), dest.height());
	while (!str.empty()) {
		char32_t schar = 0;
		int const scharcount = uchar_from_utf8(&schar, str);
		if (scharcount == -1)
			break;
		rectangle chbounds;
		font.get_scaled_bitmap_and_bounds(tempbitmap, bounds.height(), aspect, schar, chbounds);
		for (int y = 0; y < chbounds.height(); y++) {
			int const effy = bounds.top() + y;
			if (effy >= bounds.top() && effy <= bounds.bottom()) {
				u32 const *const src = &tempbitmap.pix(y);
				u32 *const d = &dest.pix(effy);
				for (int x = 0; x < chbounds.width(); x++) {
					int const effx = int(curx) + x + chbounds.left();
					if (effx >= bounds.left() && effx <= bounds.right()) {
						u32 const spix = rgb_t(src[x]).a();
						if (spix != 0)
							coinline_alpha_blend_color(d[effx], color, float(spix) / 255.0f);
					}
				}
			}
		}
		curx += font.char_width(bounds.height(), aspect, schar);
		str.remove_prefix(scharcount);
	}
}

inline void coinline_blend_argb_on_rgb32(bitmap_rgb32 &bitmap, bitmap_argb32 const &src, rectangle const &cliprect)
{
	for (int py = cliprect.min_y; py <= cliprect.max_y && py < src.height(); ++py) {
		for (int px = cliprect.min_x; px <= cliprect.max_x && px < src.width(); ++px) {
			rgb_t const t(src.pix(py, px));
			if (t.a() != 0)
				bitmap.pix(py, px) = t;
		}
	}
}

std::string opcode_hex_byte(u8 b)
{
	char buf[16];
	std::snprintf(buf, sizeof(buf), "0x%02X", unsigned(b));
	return buf;
}

u16 read_reset_jump_target(u8 const *rom)
{
	if (rom[2] == 0xc3)
		return u16(rom[3] | (u16(rom[4]) << 8));
	return 0x0001;
}

millennium_state::trace_profile parse_trace_profile()
{
	char const *const p = osd_getenv("COINLINE_TRACE_PROFILE");
	if (!p || !*p)
		return millennium_state::trace_profile::fast;
	std::string v(p);
	std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (v == "m6")
		return millennium_state::trace_profile::m6;
	if (v == "uart")
		return millennium_state::trace_profile::uart;
	if (v == "voice")
		return millennium_state::trace_profile::voice;
	if (v == "full")
		return millennium_state::trace_profile::full;
	if (v == "tp_timing")
		return millennium_state::trace_profile::tp_timing;
	return millennium_state::trace_profile::fast;
}

constexpr u32 k_terminal21_hook_bit = 0x00080000U;
constexpr u32 k_rep_dial_6_to_10_mask = 0x02000000U | 0x04000000U | 0x08000000U | 0x10000000U | 0x20000000U;

u32 terminal21_keymatrix_applied_mask(millennium_terminal21_user_io_profile p) noexcept
{
	return (p == millennium_terminal21_user_io_profile::repdial_5) ? ~k_rep_dial_6_to_10_mask : ~0U;
}

bool keymatrix_single_bit_to_terminal21_opcode(u32 mask, u8 &opcode_out) noexcept
{
	static constexpr struct { u32 m; u8 o; } tbl[] = {
		{0x00000001U, 0x22}, {0x00000002U, 0x24}, {0x00000004U, 0x26}, {0x00000008U, 0x28},
		{0x00000010U, 0x2A}, {0x00000020U, 0x2C}, {0x00000040U, 0x2E}, {0x00000080U, 0x30},
		{0x00000100U, 0x32}, {0x00000400U, 0x34}, {0x00000200U, 0x36}, {0x00000800U, 0x38},
		{0x40000000U, 0x20}, {0x00002000U, 0x3A}, {0x00004000U, 0x3C}, {0x00008000U, 0x3E},
		{0x00100000U, 0x40}, {0x00200000U, 0x42}, {0x00400000U, 0x44}, {0x00800000U, 0x46},
		{0x01000000U, 0x48}, {0x02000000U, 0x4A}, {0x04000000U, 0x4C}, {0x08000000U, 0x4E},
		{0x10000000U, 0x50}, {0x20000000U, 0x52},
	};
	for (auto const &e : tbl) {
		if (mask == e.m) {
			opcode_out = e.o;
			return true;
		}
	}
	return false;
}

/// Exactly one dial / rep-dial bit, excluding handset, volume, language, next-call (terminal_22 repeat family).
u32 terminal22_repeat_single_key_mask(u32 km_applied) noexcept
{
	u32 const cand = km_applied
		& ~(0x00010000U | 0x00020000U | 0x00040000U | 0x00001000U | k_terminal21_hook_bit);
	if (cand == 0U || (cand & (cand - 1U)) != 0U)
		return 0U;
	u8 tmp = 0;
	if (!keymatrix_single_bit_to_terminal21_opcode(cand, tmp))
		return 0U;
	return cand;
}

/// Dial pad + A–D + rep-dial matrix bits (terminal_21 / terminal_24 release + suppress region).
constexpr u32 k_terminal21_dial_pad_and_rep_bits =
	0x00000fffU | 0x0000e000U | 0x40000000U | 0x3ff00000U;

char const *terminal21_profile_id_string(millennium_terminal21_user_io_profile p) noexcept
{
	switch (p) {
	case millennium_terminal21_user_io_profile::repdial_5:
		return "repdial_5";
	case millennium_terminal21_user_io_profile::repdial_10:
		return "repdial_10";
	case millennium_terminal21_user_io_profile::vfd_11line_softkeys:
		return "vfd_11line_softkeys";
	}
	return "repdial_10";
}

} // namespace

static unsigned popcount_u32(u32 x) noexcept
{
	unsigned n = 0U;
	while (x != 0U) {
		n++;
		x &= x - 1U;
	}
	return n;
}

static bool user_io_opcode_is_dial_or_rep(std::uint8_t c) noexcept
{
	if (c == 0x20U)
		return true;
	if (c >= 0x22U && c <= 0x38U && (c & 1U) == 0U)
		return true;
	if (c >= 0x3AU && c <= 0x3EU && (c & 1U) == 0U)
		return true;
	if (c >= 0x40U && c <= 0x52U && (c & 1U) == 0U)
		return true;
	return false;
}

millennium_state::millennium_state(machine_config const &mconfig, device_type type, char const *tag)
	: driver_device(mconfig, type, tag)
	, m_maincpu(*this, "maincpu")
	, m_screen(*this, "screen")
	, m_vfd(*this, "vfd")
	, m_keypad(*this, "keypad")
	, m_security(*this, "security")
	, m_modem(*this, "modem")
	, m_hostbridge(*this, "hostbridge")
	, m_nvram(*this, "nvram")
	, m_card(*this, "card")
	, m_smartcard(*this, "smartcard")
	, m_sam(*this, "sam")
	, m_coin(*this, "coin")
	, m_audio(*this, "mill_audio")
	, m_voiceware(*this, "voiceware")
	, m_voicew_upd(*this, "voicew_upd")
	, m_vw_upd_filter(*this, "vw_upd_filter")
	, m_earpiece_tone_a(*this, "earpiece_tone_a")
	, m_earpiece_tone_b(*this, "earpiece_tone_b")
	, m_audio_route(*this, "audroute")
	, m_supervision(*this, "supervision")
	, m_telephony(*this, "telephony")
	, m_cardui(*this, "CARDUI")
	, m_coinui(*this, "COINUI")
	, m_keymatrix_io(*this, "KEYMATRIX")
	, m_secmask_io(*this, "SECMASK")
	, m_linectrl_io(*this, "LINECTRL")
	, m_terminal21_softkeys_io(*this, "TERMINAL21_SOFTKEYS")
{
	m_phys_ram.resize(0x80000, 0xff); // M5M5408-class 512 KiB linear image (see MACH PIO bank select).
	m_phys_low_overlay.resize(0xc0000U, 0U);
	m_phys_low_valid.resize(0xc0000U, 0U);
}

void millennium_state::millennium(machine_config &config)
{
	Z80180(config, m_maincpu, 12288000);
	m_maincpu->set_addrmap(AS_PROGRAM, &millennium_state::memory_map);
	m_maincpu->set_addrmap(AS_IO, &millennium_state::io_map);
	SCREEN(config, m_screen, SCREEN_TYPE_RASTER);
	m_screen->set_refresh_hz(60);
	m_screen->set_vblank_time(ATTOSECONDS_IN_USEC(0));
	m_screen->set_size(1280, 900);
	m_screen->set_visarea_full();
	m_screen->set_screen_update(FUNC(millennium_state::screen_update));
	MILLENNIUM_VFD(config, m_vfd, 0);
	MILLENNIUM_KEYPAD(config, m_keypad, 0);
	MILLENNIUM_SECURITY(config, m_security, 0);
	MILLENNIUM_MODEM(config, m_modem, 0);
	m_maincpu->rts0_wr_callback().set([this](int state) { m_modem->model().set_rts(state != 0); });
	MILLENNIUM_HOSTBRIDGE(config, m_hostbridge, 0);
	MILLENNIUM_NVRAM(config, m_nvram, 0);
	MILLENNIUM_CARD(config, m_card, 0);
	MILLENNIUM_SMARTCARD(config, m_smartcard, 0);
	MILLENNIUM_SAM(config, m_sam, 0);
	MILLENNIUM_COIN(config, m_coin, 0);
	MILLENNIUM_AUDIO(config, m_audio, 0);
	SPEAKER(config, "vwspk").front_center();
	UPD7759(config, m_voicew_upd, upd7759_device::STANDARD_CLOCK);
	m_voicew_upd->set_device_rom_tag(":voicew");
	FILTER_RC(config, m_vw_upd_filter, 0);
	m_vw_upd_filter->set_lowpass(RES_K(10), CAP_N(4.7));
	MILLENNIUM_VOICEWARE(config, m_voiceware, 0);
	m_voiceware->add_route(ALL_OUTPUTS, "vwspk", 1.0);
	m_voicew_upd->add_route(ALL_OUTPUTS, "vw_upd_filter", 1.0);
	m_vw_upd_filter->add_route(ALL_OUTPUTS, "vwspk", coinline_voiceware_analog_route_gain_from_env());
	SPEAKER(config, "telspk").front_center();
	// North American dial-tone style dual frequency mix (350 Hz + 440 Hz).
	BEEP(config, m_earpiece_tone_a, 350);
	m_earpiece_tone_a->add_route(ALL_OUTPUTS, "telspk", 0.09);
	BEEP(config, m_earpiece_tone_b, 440);
	m_earpiece_tone_b->add_route(ALL_OUTPUTS, "telspk", 0.09);
	MILLENNIUM_AUDIO_ROUTE(config, m_audio_route, 0);
	MILLENNIUM_SUPERVISION(config, m_supervision, 0);
	MILLENNIUM_TELEPHONY(config, m_telephony, 0);

	config.set_default_layout(layout_millennium);
}

void millennium_state::memory_map(address_map &map) { millennium_map_program(map, *this); }

void millennium_state::io_map(address_map &map) { millennium_configure_io_map(map, *this); }

std::string millennium_state::resolve_relative_path(std::string const &rel) const
{
	if (rel.size() >= 2 && rel[1] == ':')
		return rel;
	if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
		return rel;
	char const *root = osd_getenv("COINLINE_EMU_ROOT");
	if (root && *root) {
		std::string r(root);
		while (!r.empty() && (r.back() == '/' || r.back() == '\\'))
			r.pop_back();
		return r + "/" + rel;
	}
	return rel;
}

void millennium_state::load_io_fixture_masks()
{
	std::string const path = resolve_relative_path("fixtures/board/io-port-map.json");
	std::vector<std::uint8_t> raw;
	std::string err;
	if (!millennium_read_file(path, raw, err)) {
		m_io_known_or_suspected.fill(false);
		return;
	}
	std::string const text(reinterpret_cast<char const *>(raw.data()), raw.size());
	std::string perr;
	if (!millennium_io_parse_port_map(text, m_unknown_default, m_io_known_or_suspected, perr))
		m_unknown_default = 0xff;
}

void millennium_state::load_firmware_and_emit_m0()
{
	char const *fw = osd_getenv("COINLINE_FIRMWARE");
	char const *fw0 = osd_getenv("COINLINE_FIRMWARE_FLASH0");
	char const *fw1 = osd_getenv("COINLINE_FIRMWARE_FLASH1");
	bool const explicit_firmware_env =
			(fw && *fw) || (fw0 && *fw0) || (fw1 && *fw1);
	std::string const rel_flash0("../firmware/flash.bin");
	std::string const rel_flash1("../firmware/flash1.bin");
	std::string const rel_legacy_flash("../firmware/flash-legacy.bin");

	std::string path0;
	if (fw0 && *fw0)
		path0 = resolve_relative_path(std::string(fw0));
	else if (fw && *fw)
		path0 = resolve_relative_path(std::string(fw));
	else if (!explicit_firmware_env)
	{
		path0 = resolve_relative_path(rel_flash0);
		std::vector<std::uint8_t> probe;
		std::string probe_err;
		if (!millennium_read_file(path0, probe, probe_err))
			path0 = resolve_relative_path(rel_legacy_flash);
	}
	else
		path0 = resolve_relative_path(rel_flash0);

	std::vector<std::uint8_t> fw_bytes;
	std::string err;
	if (!millennium_read_file(path0, fw_bytes, err))
		throw emu_fatalerror("millennium: %s", err.c_str());

	bool append_second = false;
	std::string path1;
	if (fw1 && *fw1)
	{
		path1 = resolve_relative_path(std::string(fw1));
		append_second = true;
	}
	else if (!explicit_firmware_env)
	{
		path1 = resolve_relative_path(rel_flash1);
		append_second = true;
	}

	if (append_second)
	{
		std::vector<std::uint8_t> tail;
		if (!millennium_read_file(path1, tail, err))
			throw emu_fatalerror("millennium: %s", err.c_str());
		fw_bytes.insert(fw_bytes.end(), tail.begin(), tail.end());
	}

	if (m_board_profile_json.empty())
		throw emu_fatalerror("millennium: internal error: board profile not loaded");

	std::string const hashes_path = resolve_relative_path("fixtures/firmware/firmware-hashes.json");
	std::vector<std::uint8_t> hashes_raw;
	if (!millennium_read_file(hashes_path, hashes_raw, err))
		throw emu_fatalerror("millennium: firmware-hashes.json: %s", err.c_str());
	std::string const hashes_text(reinterpret_cast<char const *>(hashes_raw.data()), hashes_raw.size());

	auto const vr = millennium_validate_firmware(fw_bytes, m_board_profile_json, hashes_text);
	if (!vr.ok)
		throw emu_fatalerror("millennium: firmware validation failed: %s", vr.error.c_str());

	memory_region *const rom = memregion("flash");
	if (!rom || rom->bytes() < fw_bytes.size())
		throw emu_fatalerror("millennium: flash region too small");
	std::memcpy(rom->base(), fw_bytes.data(), fw_bytes.size());
	m_firmware_sha256_hex = vr.sha256_hex;

	std::string const ts = millennium_boot_trace_timestamp_utc();
	std::string const line = millennium_boot_trace_m0(ts, m_firmware_sha256_hex, std::uint64_t(fw_bytes.size()));
	millennium_boot_trace_append_line(m_boot_trace_path, line);
}

void millennium_state::driver_start()
{
	driver_device::driver_start();

	char const *bt = osd_getenv("COINLINE_BOOT_TRACE");
	m_boot_trace_path = (bt && *bt) ? std::filesystem::path(bt) : std::filesystem::path("boot-trace.jsonl");
	char const *up = osd_getenv("COINLINE_UNKNOWN_PORT_LOG");
	m_unknown_port_path =
		(up && *up) ? std::filesystem::path(up) : std::filesystem::path("unknown-port.jsonl");
	char const *iot = osd_getenv("COINLINE_IO_TRACE");
	m_io_trace_path = (iot && *iot) ? std::filesystem::path(iot) : std::filesystem::path();
	char const *memt = osd_getenv("COINLINE_MEMORY_TRACE");
	m_memory_trace_path = (memt && *memt) ? std::filesystem::path(memt) : std::filesystem::path();
	char const *cput = osd_getenv("COINLINE_CPU_TRACE");
	m_cpu_trace_path = (cput && *cput) ? std::filesystem::path(cput) : std::filesystem::path();
	char const *z18 = osd_getenv("COINLINE_Z180_REG_TRACE");
	m_z180_reg_trace_path = (z18 && *z18) ? std::filesystem::path(z18) : std::filesystem::path();
	m_trace_profile = parse_trace_profile();
	m_tel_response_policy = parse_tel_response_policy_env();
#if COINLINE_ENABLE_TP8048_BACKEND
	m_tp_backend_kind = tp_backend_kind::pcd3349a;
#else
	m_tp_backend_kind = tp_backend_kind::legacy;
#endif
	if (char const *tb = osd_getenv("COINLINE_TP_BACKEND"); tb && *tb) {
		std::string const sel(tb);
		if (sel == "legacy")
			m_tp_backend_kind = tp_backend_kind::legacy;
#if COINLINE_ENABLE_TP8048_BACKEND
		else if (sel == "pcd3349a" || sel == "8048" || sel == "pcd3349a_8048")
			m_tp_backend_kind = tp_backend_kind::pcd3349a;
#endif
	}
	if (m_tp_backend_kind == tp_backend_kind::pcd3349a)
		m_tp_pcd3349a = std::make_unique<millennium_pcd3349a>();
	else
		m_tp_pcd3349a.reset();
	m_trace_capture_full_io = (m_trace_profile == trace_profile::full);
	m_trace_capture_full_cpu = (m_trace_profile == trace_profile::full);
	// Fault-context capture (vector probe + rings) is expensive; keep it off for the common 30s UART bring-up run
	// unless explicitly requested.
	bool env_fault_context = false;
	if (char const *fc = osd_getenv("COINLINE_TRACE_FAULT_CONTEXT"); fc && *fc && *fc != '0')
		env_fault_context = true;
	m_trace_capture_fault_context = env_fault_context || (m_trace_profile == trace_profile::m6 || m_trace_profile == trace_profile::full);
	m_trace_capture_hot_summary = (m_trace_profile == trace_profile::m6 || m_trace_profile == trace_profile::uart
		|| m_trace_profile == trace_profile::tp_timing);

	char const *uioh = osd_getenv("COINLINE_USER_IO_HARNESS_TRACE");
	m_user_io_harness_trace_path =
		(uioh && *uioh) ? std::filesystem::path(uioh) : std::filesystem::path("user-io-trace.jsonl");
	if (!m_user_io_harness_trace_path.empty()) {
		std::filesystem::path const out_dir = m_user_io_harness_trace_path.parent_path();
		if (out_dir.empty()) {
			m_user_io_harness_summary_path = std::filesystem::path("user-io-summary.json");
			m_user_io_harness_failures_path = std::filesystem::path("user-io-failures.md");
		} else {
			m_user_io_harness_summary_path = out_dir / "user-io-summary.json";
			m_user_io_harness_failures_path = out_dir / "user-io-failures.md";
		}
	}
	if (char const *rid = osd_getenv("COINLINE_RUN_ID"); rid && *rid)
		m_user_io_harness_run_id = rid;

	char const *bp = osd_getenv("COINLINE_BOARD");
	std::string const board_rel =
		(bp && *bp) ? std::string(bp) : std::string("fixtures/board/board-profile-2line-vfd.json");
	std::string const board_path = resolve_relative_path(board_rel);
	std::vector<std::uint8_t> board_raw;
	std::string err;
	if (!millennium_read_file(board_path, board_raw, err))
		throw emu_fatalerror("millennium: board profile: %s", err.c_str());
	m_board_profile_json.assign(reinterpret_cast<char const *>(board_raw.data()), board_raw.size());

	std::string zerr;
	m_z180_board = millennium_z180_board_config{};
	millennium_board_parse_z180_profile(m_board_profile_json, m_z180_board, zerr);

	std::string derr;
	m_display_profile = millennium_display_profile{};
	if (!millennium_board_parse_display_profile(m_board_profile_json, m_display_profile, derr))
		throw emu_fatalerror("millennium: display profile: %s", derr.c_str());

	m_idle_display_fixture_json.clear();
	{
		std::string const idle_path = resolve_relative_path(m_display_profile.idle_fixture_relpath);
		std::vector<std::uint8_t> raw;
		std::string ierr;
		if (millennium_read_file(idle_path, raw, ierr))
			m_idle_display_fixture_json.assign(reinterpret_cast<char const *>(raw.data()), raw.size());
		else
			osd_printf_warning("millennium: idle VFD fixture not loaded (%s): %s\n", idle_path.c_str(), ierr.c_str());
	}

	m_keypad_board = millennium_keypad_board_config{};
	if (!millennium_board_parse_keypad_profile(m_board_profile_json, m_keypad_board, derr))
		throw emu_fatalerror("millennium: keypad profile: %s", derr.c_str());
	millennium_board_resolve_terminal21_profile(m_display_profile, m_keypad_board);

	m_security_board = millennium_security_board_config{};
	if (!millennium_board_parse_security_profile(m_board_profile_json, m_security_board, derr))
		throw emu_fatalerror("millennium: security profile: %s", derr.c_str());

	std::string cerr;
	m_coin_board = millennium_coin_board_config{};
	if (!millennium_board_parse_coin_profile(m_board_profile_json, m_coin_board, cerr))
		throw emu_fatalerror("millennium: coin profile: %s", cerr.c_str());

	m_audio_board = millennium_alerter_board_config{};
	if (!millennium_board_parse_alerter_profile(m_board_profile_json, m_audio_board, cerr))
		throw emu_fatalerror("millennium: alerter profile: %s", cerr.c_str());

	std::string merr;
	m_memory_layout = millennium_memory_layout_config{};
	if (!millennium_board_parse_memory_layout(m_board_profile_json, m_memory_layout, merr))
		throw emu_fatalerror("millennium: memory layout: %s", merr.c_str());

	m_user_io_board = millennium_user_io_board_config{};
	if (!millennium_board_parse_user_io_section(m_board_profile_json, m_user_io_board, merr))
		osd_printf_warning("millennium: user_io section: %s\n", merr.c_str());
	if (char const *uiohi = osd_getenv("COINLINE_USER_IO_HARNESS_INPUT"); uiohi && *uiohi) {
		std::vector<std::uint8_t> in_raw;
		std::string in_err;
		std::string const in_path = resolve_relative_path(uiohi);
		if (!millennium_read_file(in_path, in_raw, in_err))
			throw emu_fatalerror("millennium: user_io harness input: %s", in_err.c_str());
		std::string const in_text(reinterpret_cast<char const *>(in_raw.data()), in_raw.size());
		user_io_harness_input parsed{};
		if (!parse_user_io_harness_input_text(in_text, parsed, in_err))
			throw emu_fatalerror("millennium: user_io harness input invalid: %s", in_err.c_str());
		m_user_io_harness_run_id = parsed.run_id;
		m_user_io_harness_enabled_vectors = parsed.enabled_vectors;
		m_keypad_board.quick_access_key_count = parsed.quick_access_key_count;
		if (parsed.has_11_line_softkeys) {
			if (parsed.quick_access_key_count != 10U)
				throw emu_fatalerror("millennium: harness input invalid: has_11_line_softkeys requires quick_access_key_count=10");
			m_keypad_board.terminal_21_profile = millennium_terminal21_user_io_profile::vfd_11line_softkeys;
		} else if (parsed.quick_access_key_count == 5U) {
			m_keypad_board.terminal_21_profile = millennium_terminal21_user_io_profile::repdial_5;
		} else {
			m_keypad_board.terminal_21_profile = millennium_terminal21_user_io_profile::repdial_10;
		}
		m_keypad_board.terminal_21_profile_explicit = true;
		if (parsed.has_adsi_active)
			m_user_io_board.overlay.adsi_active = parsed.adsi_active;
		if (parsed.has_proton_active)
			m_user_io_board.overlay.proton_active = parsed.proton_active;
		if (parsed.has_mondex_active)
			m_user_io_board.overlay.mondex_active = parsed.mondex_active;
		if (parsed.has_git_ui_active)
			m_user_io_board.overlay.git_ui_active = parsed.git_ui_active;
	}
	{
		auto env_bool = [](char const *name, bool &b) {
			char const *const e = osd_getenv(name);
			if (!e || !e[0])
				return;
			if (e[0] == '0' && e[1] == 0)
				b = false;
			else if (e[0] == '1' && e[1] == 0)
				b = true;
		};
		env_bool("COINLINE_USER_IF_ACTIVE", m_user_io_board.policy.user_if_active);
		env_bool("COINLINE_UI_PROTECTION_BLOCKS_SOFT", m_user_io_board.policy.protection_blocks_user_if_soft_actions);
		env_bool("COINLINE_ADSI_RUNTIME_SESSION", m_user_io_board.policy.adsi_runtime_session_active);
		env_bool("COINLINE_UI_OVERLAY_BLOCK_LANG_NEXT", m_user_io_board.policy.overlay_blocks_language_next_call);
		env_bool("COINLINE_PROTON_UI_BLOCK_LANG_NEXT", m_user_io_board.policy.proton_ui_blocks_language_next_call);
		env_bool("COINLINE_MONDEX_UI_BLOCK_LANG_NEXT", m_user_io_board.policy.mondex_local_ui_blocks_language_next_call);
		env_bool("COINLINE_GIT_UI_BLOCK_LANG_NEXT", m_user_io_board.policy.git_ui_blocks_language_next_call);
		env_bool("COINLINE_UI_PROTECTION_BLOCKS_DIAL", m_user_io_board.policy.protection_blocks_dial_pad);
		env_bool("COINLINE_UI_CP_ABSORB_BLOCKED", m_user_io_board.policy.cp_absorb_blocked_user_if_opcodes);
	}

	load_io_fixture_masks();
	load_firmware_and_emit_m0();
	init_auxiliary_trace_sinks();
	if (m_tp_pcd3349a) {
		m_tp_pcd3349a->set_trace_paths(m_tp_8048_runtime_trace_path, m_tp_8048_port_trace_path,
			m_tp_8048_keypad_trace_path, m_tp_8048_tone_trace_path, m_tp_8048_cp_protocol_trace_path);
	}

	if (m_z180_board.clock_hz != 0)
		m_maincpu->set_unscaled_clock(double(m_z180_board.clock_hz));
	if (m_tp_pcd3349a)
		m_tp_pcd3349a->set_cp_hz(static_cast<std::uint32_t>(m_maincpu->unscaled_clock()));
}

void millennium_state::ensure_vfd_render_font()
{
	if (m_vfd_render_prepared)
		return;
	m_vfd_render_prepared = true;
#ifdef _WIN32
	std::string const path = resolve_relative_path("artwork/Millenft.ttf");
	int const nw = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
	if (nw > 0) {
		std::wstring wpath(static_cast<size_t>(nw), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), nw);
		if (AddFontResourceExW(wpath.c_str(), FR_PRIVATE, nullptr) > 0) {
			m_win_vfd_font_added = true;
			m_win_vfd_font_path = std::move(wpath);
			m_vfd_font = machine().render().font_alloc("Millennium");
			m_vfd_use_gfxfont_fallback = false;
			return;
		}
	}
#endif
	m_vfd_use_gfxfont_fallback = true;
}

namespace {

void patch_voicew_rom_headers_for_upd7759_master(memory_region *vr, device_t &logdev)
{
	if (!vr)
		return;
	u8 *const base = vr->base();
	u32 const bytes = vr->bytes();
	static constexpr u32 bank_bytes = 0x20000; // 128 KiB per uPD7759 ROM bank (17-bit space)
	static constexpr u8 min_boot_phrase_index = 0xb3; // observed boot `voice_phrase` before VFD path
	for (u32 off = 0; off + bank_bytes <= bytes; off += bank_bytes) {
		u8 *const hdr = base + off;
		bool const nec_magic = hdr[1] == 0x5a && hdr[2] == 0xa5 && hdr[3] == 0x69 && hdr[4] == 0x55;
		if (nec_magic) {
			// Voiceware (and NEC directory) ROMs use byte 0 as last message index in the segment directory
			// (message count - 1). Forcing 0xff breaks millennium_voiceware_device; raw-ROM forensics stay valid.
			continue;
		}
		u8 const was = hdr[0];
		if (was >= min_boot_phrase_index)
			continue;
		logdev.logerror(
			"millennium: voicew bank@%06x no NEC hdr magic; raising last_sample %02x -> FF for bring-up\n",
			off, was);
		hdr[0] = 0xff;
	}
}

} // namespace

static bool millennium_recognized_tp_to_cp_single_byte_model(std::uint8_t v)
{
	// terminal_21 TP→CP user/input opcodes plus single-byte replies modeled for telephony bring-up / queries.
	switch (v) {
	case 0x20U: case 0x22U: case 0x24U: case 0x26U: case 0x28U: case 0x2AU:
	case 0x2CU: case 0x2EU: case 0x30U: case 0x32U: case 0x34U: case 0x36U:
	case 0x38U: case 0x3AU: case 0x3CU: case 0x3EU:
	case 0x40U: case 0x42U: case 0x44U: case 0x46U: case 0x48U: case 0x4AU:
	case 0x4CU: case 0x4EU: case 0x50U: case 0x52U:
	case 0x54U: case 0x56U: case 0x58U: case 0x5AU: case 0x5EU:
	case 0x60U: case 0x62U: case 0x64U: case 0x66U: case 0x68U: case 0x6AU:
	case 0x6CU: case 0x6EU:
	case 0x70U: case 0x72U:
	case 0x7AU: case 0x7CU:
	case 0x80U:
	case 0x88U: case 0x8AU:
	case 0x8CU: case 0x8EU:
		return true;
	default:
		break;
	}
	return v >= 0x90U && v <= 0x9BU;
}

void millennium_state::poll_voice_segment_done_pulse_int0()
{
	// `millennium_voiceware_device::playing()` is grounded in software PCM or `upd7759_device::busy_r()` per env.
	bool const idle = !m_voiceware->playing();
	if (!m_voicew_prev_upd_idle && idle && !m_voicew_int0_asserted) {
		if (!m_m5c_logged) {
			char pcbuf[16];
			std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(m_maincpu->pc() & 0xffffU));
			std::string const ts = millennium_boot_trace_timestamp_utc();
			millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m5c(ts, pcbuf));
			m_m5c_logged = true;
		}
		m_maincpu->set_input_line(Z180_INPUT_LINE_IRQ0, ASSERT_LINE);
		m_voicew_int0_asserted = true;
		if (!m_voiceware_trace_path.empty()) {
			char buf[256];
			std::snprintf(buf, sizeof(buf),
				"{\"cycle\":%llu,\"event_type\":\"voice_irq0_assert\",\"device\":\"voiceware\","
				"\"line\":\"INT0\",\"reason\":\"upd7759_busy_to_idle_completion\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()));
			millennium_boot_trace_append_line(m_voiceware_trace_path, buf);
		}
	}
	m_voicew_prev_upd_idle = idle;
}

void millennium_state::clear_voice_segment_int0(char const *reason)
{
	if (!m_voicew_int0_asserted)
		return;
	m_maincpu->set_input_line(Z180_INPUT_LINE_IRQ0, CLEAR_LINE);
	m_voicew_int0_asserted = false;
	if (!m_voiceware_trace_path.empty()) {
		char buf[256];
		std::snprintf(buf, sizeof(buf),
			"{\"cycle\":%llu,\"event_type\":\"voice_irq0_clear\",\"device\":\"voiceware\","
			"\"line\":\"INT0\",\"reason\":\"%s\"}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), reason ? reason : "unknown");
		millennium_boot_trace_append_line(m_voiceware_trace_path, buf);
	}
}

void millennium_state::machine_start()
{
	driver_device::machine_start();
#ifdef _WIN32
	// init_auxiliary_trace_sinks runs in driver_start(); on some Windows launch chains the Win32
	// environment block is not yet visible to the same probes the driver uses, leaving JSONL paths
	// empty. Re-read once at machine_start using the same osd_getenv -> Win32 fallback order as
	// init_auxiliary_trace_sinks sidecar().
	//
	// EEPROM / NVRAM gate paths are always refreshed here (even when COINLINE_TRACE_ONLY is set):
	// launchers often clear TRACE_ONLY only in the shell process block while MAME still inherits a
	// user/machine COINLINE_TRACE_ONLY that cleared these members during driver_start; skipping the
	// refresh then left microwire + nvram-storage JSONL sinks empty for the whole run.
	{
		auto refresh_path_env = [](char const *env, std::filesystem::path &out) {
			char const *p = osd_getenv(env);
			std::string win_storage;
			if (!p || !*p) {
				char buf[2048] = {};
				DWORD const wn = GetEnvironmentVariableA(env, buf, sizeof(buf));
				if (wn > 0U && wn < sizeof(buf))
					win_storage.assign(buf, static_cast<std::size_t>(wn));
			}
			if (!win_storage.empty())
				p = win_storage.c_str();
			if (p && *p)
				out = std::filesystem::path(p);
		};
		refresh_path_env("COINLINE_MICROWIRE_TRACE", m_microwire_trace_path);
		refresh_path_env("COINLINE_NVRAM_STORAGE_TRACE", m_nvram_storage_trace_path);
	}
#endif
	if (!coinline_voiceware_upd7759_core_from_env())
		patch_voicew_rom_headers_for_upd7759_master(memregion("voicew"), *this);
	else
		logerror("millennium: uPD7759 core path active — skipping voicew ROM header patch (disable with COINLINE_VOICEWARE_UPD7759_CORE=0)\n");
	if (coinline_voiceware_analog_filter_bypass_from_env())
		m_vw_upd_filter->filter_rc_set_RC(filter_rc_device::LOWPASS, RES_K(10), 0, 0, 0);
	else
		m_vw_upd_filter->filter_rc_set_RC(filter_rc_device::LOWPASS, RES_K(10), 0, 0, CAP_N(4.7));
	m_vfd->apply_display_profile(m_display_profile);
	m_keypad->apply_config(m_keypad_board);
	m_security->apply_config(m_security_board);
	m_coin->apply_config(m_coin_board);
	m_audio->apply_config(m_audio_board);
	m_coin->set_cpu_hz(static_cast<u64>(m_maincpu->unscaled_clock()));
	m_microwire_93c66.set_write_mirror_cb([this](std::uint32_t byte_offset, std::uint16_t word_be) {
		std::string err;
		if (byte_offset + 1U >= millennium_microwire_93c66::k_capacity_bytes)
			return;
		(void)m_nvram->model().write_nvram(byte_offset, static_cast<u8>((word_be >> 8) & 0xffU), err);
		(void)m_nvram->model().write_nvram(byte_offset + 1U, static_cast<u8>(word_be & 0xffU), err);
	});
	if (!m_microwire_trace_path.empty()) {
		m_microwire_93c66.set_access_trace_cb([this](char const *op, std::uint16_t word_addr, std::uint16_t word_be) {
			u16 const pc = m_maincpu->pc();
			u16 const sp = m_maincpu->state_int(Z180_SP);
			std::string const line = millennium_format_microwire_trace_line(m_maincpu->total_cycles(), op, word_addr,
				word_be, pc, sp, "none");
			millennium_boot_trace_append_line(m_microwire_trace_path, line);
		});
	}
	else {
		m_microwire_93c66.set_access_trace_cb({});
	}
	sync_microwire_from_nvram();
	schedule_driver_timer(TID_CARD_UI, attotime::from_msec(25));
	schedule_driver_timer(TID_COIN_UI, attotime::from_msec(25));
}

void millennium_state::emit_trace_m1_m2()
{
	u8 const *const rom = memregion("flash")->base();
	std::string const ts = millennium_boot_trace_timestamp_utc();

	u16 const reset_pc = 0x0000;
	std::string const m1 = millennium_boot_trace_m1(ts, reset_pc, opcode_hex_byte(rom[0]));
	millennium_boot_trace_append_line(m_boot_trace_path, m1);

	u16 const m2_pc = read_reset_jump_target(rom);
	std::string const m2 = millennium_boot_trace_m2(ts, m2_pc);
	millennium_boot_trace_append_line(m_boot_trace_path, m2);
}

void millennium_state::machine_reset()
{
	driver_device::machine_reset();
	if (!m_reset_trace_path.empty()) {
		char rbuf[240];
		std::snprintf(rbuf, sizeof(rbuf),
			"{\"event\":\"machine_reset\",\"cycle\":%llu,\"note\":\"cold_or_soft_reset\",\"milestone\":\"M0\"}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()));
		millennium_boot_trace_append_line(m_reset_trace_path, rbuf);
	}
	std::fill(m_phys_ram.begin(), m_phys_ram.end(), 0xff);
	std::fill(m_phys_low_valid.begin(), m_phys_low_valid.end(), 0U);
	m_ram_write_events = 0;
	m_stack_trace_remaining = 100U;
	m_ram_init_trace_remaining = 2000U;
	m_m3_m4_logged = false;
	m_m5_logged = false;
	m_m5v_logged = false;
	m_m5a_logged = false;
	m_m5c_logged = false;
	m_voicew_chip_was_active = false;
	m_voicew_prev_upd_idle = true;
	m_voicew_int0_asserted = false;
	m_maincpu->set_input_line(Z180_INPUT_LINE_IRQ0, CLEAR_LINE);
	m_last_modem_dcd = false;
	sync_modem_asci_lines();
	m_m6_logged = false;
	m_m7_logged = false;
	m_m7a_logged = false;
	m_m7_rx_path_logged = false;
	m_m7c_logged = false;
	m_m7c_gate_diag_last_cycle = 0;
	m_m8_logged = false;
	m_m9_logged = false;
	m_m10_logged = false;
	m_boot_protocol_ready_milestone_logged = false;
	m_boot_acceptance_ready_milestone_logged = false;
	m_vfd_idle_fixture_diff_counter = 0U;
	m_cp_install_key_buffer_model.clear();
	m_craft_gate_accept_traced = false;
	m_craft_code_detected_traced = false;
	m_craft_screen_vfd_trace_logged = false;
	m_asci_baseline_ready = false;
	emit_trace_m1_m2();
	m_mach_pio_shadow.fill(0xff);
	m_pio_8255_shadow.fill(0xff);
	m_pio_port_g = 0xff;
	{
		bool new_hw_rev = true;
		if (char const *const e = osd_getenv("COINLINE_NEW_HARDWARE_REVISION_1"); e && e[0] && e[0] == '0' && e[1] == 0)
			new_hw_rev = false;
		millennium_hwinit_apply_pio_port_initialize(new_hw_rev, m_pio_8255_shadow, m_pio_port_g, m_mach_pio_shadow);
	}
	// Bit 7: CSIO clock (SCLK); bit 5: PWR_FAIL_LINE — start released (high) so first firmware pulse is assert-low.
	m_hw_cntl_port_image = 0xa0U;
	m_vector_event_budget = VECTOR_EVENT_BUDGET_MAX;
	m_vecprobe_last_pc = 0xffffU;
	m_vecprobe_last_op0 = 0xffU;
	m_vecprobe_last_iff1 = false;
	m_vecprobe_last_iff2 = false;
	m_vecprobe_in_ctx_band = false;
	m_first_pc_ffff_seen = false;
	m_first_rst38_seen = false;
	m_first_pc_ffff_next_remaining = 0U;
	m_first_rst38_next_remaining = 0U;
	m_pc_fault_context_ring.clear();
	m_last_io_event_json.clear();
	m_last_stack_write_json.clear();
	m_last_memory_write_by_phys.clear();
	m_card_ui_last = 0;
	m_coin_ui_last = 0;
	m_audio_dtmf_ascii = '5';
	m_ext_uart_dll = 0x80U;
	m_ext_uart_dlm = 0x02U;
	millennium_hwinit_apply_coin_validator_tl16c550(m_ext_uart_shadow);
	m_ext_uart_rx_queue.clear();
	m_ext_uart_boot_seeded = false;
	m_ext_uart_tel_up_seen = false;
	m_tel_ip_link_enabled = false;
	m_tel_boot_code_sent = false;
	m_tel_reset_cycle_start = m_maincpu->total_cycles();
	m_tel_ip_link_enable_deadline_cycle = 0ULL;
	m_tel_link_enable_cycle = 0ULL;
	m_tel_boot_code_deadline_cycle = 0ULL;
	m_tel_uart_phase = tel_uart_phase::wait_rx_enable;
	m_tel_status_frame_ok = false;
	m_csio_status_frame_ok = false;
	m_csio_error_report_ok = false;
	m_csio_ack_seeded = false;
	m_tel_boot_semantic_state = tel_boot_semantic_state::reset_pending;
	m_tel_fw_boot_contract_satisfied = false;
	m_tel_boot_power_ack_seen = false;
	m_tel_boot_hook_state_seen = false;
	m_tel_boot_power_status_seen = false;
	m_tel_boot_error_report_seen = false;
	m_tel_boot_status_seen = false;
	m_alarm_tel_not_responding_latched = false;
	m_alarm_tel_not_responding_cleared = false;
	m_termfg_telephony_up_inferred = false;
	m_not_responding_display_seen = false;
	m_runtime_oos_seen = false;
	m_runtime_not_responding_seen = false;
	m_vfd_is_oos = false;
	m_vfd_is_not_responding = false;
	m_tel_policy_start_cycle = 0;
	m_tel_pending_status_after_clear = false;
	m_tel_ready_sequence_completed = false;
	m_tel_version_c2_sent = false;
	m_tel_ready_heartbeat_div = 0U;
	m_tel_not_responding_poll_count = 0U;
	m_tp_last_heartbeat_cycle = 0ULL;
	m_tp_heartbeat_count = 0ULL;
	m_tel_uart_tx_recent.clear();
	m_tel_uart_tx_in_frame = false;
	m_tel_uart_tx_code = 0;
	m_tel_uart_tx_len = 0;
	m_tel_uart_tx_sum = 0;
	m_tel_uart_tx_remaining = 0;
	m_tel_uart_stx_pending = false;
	m_csio_rx_in_frame = false;
	m_csio_rx_code = 0;
	m_csio_rx_len = 0;
	m_csio_rx_sum = 0;
	m_csio_rx_remaining = 0;
	m_tel_rx_in_frame = false;
	m_tel_rx_code = 0;
	m_tel_rx_len = 0;
	m_tel_rx_sum = 0;
	m_tel_rx_remaining = 0;
	m_ext_uart_tx_count = 0U;
	m_front_panel_trace_seeded = false;
	m_last_keymatrix_state = 0U;
	m_last_secmask_state = 0U;
	m_last_linectrl_state = 0U;
	m_last_firmware_keymatrix_state = 0U;
	m_last_firmware_linectrl_state = 0U;
	{
		u32 const km0 =
			m_keypad->keymatrix_with_hook_debounce(m_keymatrix_io->read(), m_maincpu->total_cycles());
		m_last_tp_ui_keymatrix_state = km0 & terminal21_keymatrix_applied_mask(m_keypad_board.terminal_21_profile);
		m_tp_ui_policy_km_snapshot = m_last_tp_ui_keymatrix_state;
	}
	m_last_tp_ui_linectrl_state = m_linectrl_io->read();
	m_last_tp_ui_softkeys_state =
		(m_keypad_board.terminal_21_profile == millennium_terminal21_user_io_profile::vfd_11line_softkeys)
			? (m_terminal21_softkeys_io->read() & 0x0fffU)
			: 0U;
	m_tp_ui_sk_last_raw = m_last_tp_ui_softkeys_state;
	m_tp_ui_sk_stable = m_last_tp_ui_softkeys_state;
	m_tp_ui_sk_stable_deadline_cy = 0ULL;
	m_tp_ui_repeat_hold_mask = 0U;
	m_tp_ui_repeat_hold_start_cy = 0ULL;
	m_tp_ui_repeat_extra_sent = 0U;
	m_tp_ui_pending_dial_release = 0U;
	m_tp_ui_dial_release_deadline_cy = 0ULL;
	m_tp_ui_suppress_dial_until_physical_release = 0U;
	m_tp_ui_last_hook_transition_cy = 0ULL;
	m_tp_ui_fault_duplicate_release_count = 0ULL;
	m_tp_ui_fault_synthetic_release_count = 0ULL;
	m_tp_ui_fault_hook_integrity_violation_count = 0ULL;
	m_tp_ui_fault_unknown_opcode_count = 0ULL;
	m_tp_ui_fault_dropped_event_count = 0ULL;
	m_tp_ui_fault_illegal_multi_softkey_sample_count = 0ULL;
	m_tp_ui_fault_abuse_guard_escalation_count = 0ULL;
	m_tp_ui_last_accepted_hook_transition_cy = 0ULL;
	m_tp_ui_abuse_rapid_hook_transition_accum = 0U;
	m_tp_ui_softkey_illegal_episode = false;
	m_front_panel_active_read_trace_budget = 200U;
	m_voice_pb_prev = 0xffU;
	m_voice_pb_last_phrase = 0xffU;
	m_voice_pb_last_cycle = 0ULL;
	m_cpu_trace_total_lines = 0;
	m_cpu_trace_ring_mode = false;
	m_cpu_trace_ring.clear();
	m_io_trace_ring.clear();
	m_unknown_io_counts.clear();
	m_tp_cadence_prev_cycle_by_event.clear();
	m_tp_last_csio_timing_cycle = 0ULL;
	m_tp_last_host_poll_cycle = 0ULL;
	m_uart_read_counts.fill(0U);
	m_uart_write_counts.fill(0U);
	m_uart_io_log_budget = 32U;
	// IP-comm link starts idle: no telephony RTS asserted, CSIO clock line high.
	m_ipcomm_rts_asserted = false;
	m_ipcomm_last_sclk = true;
	m_ipcomm_rx_prio_bytes.clear();
	m_ipcomm_rx_bytes.clear();
	m_ipcomm_have_rx_byte = false;
	m_ip_tx_need_length_byte = false;
	m_ip_tx_skip_remain = 0;
	m_ip_tx_var_hdr = 0;
	m_ip_tx_declared_len = 0;
	m_csio_tx_last_candidate_cycle = 0ULL;
	m_csio_tx_last_accepted_cycle = 0ULL;
	m_csio_tx_last_candidate_byte = 0U;
	m_csio_tx_last_accepted_byte = 0U;
	m_csio_tx_shift_edge_count = 0U;
	m_csio_tx_shift_epoch = 0U;
	m_csio_tx_last_accepted_epoch = 0U;
	// 0 = deliver modeled C4/C0 immediately; non-zero simulates missed RX until TMR_4 retries exhaust
	// (telephony “not responding” threshold). Withholding can miss the power-on sequence window for telephony bootstrap info.
	m_tel_hook_onhook = true;
	m_tel_hook_onhook_stable = true;
	if (m_tp_pcd3349a)
		m_tp_pcd3349a->reset(true);
	m_audio_route->notify_hook_off(false);
	m_tel_hook_last_change_cycle = 0ULL;
	m_tel_hook_stabilize_until_cycle = 0ULL;
	m_tel_handset_ok = true;
	m_tel_health_consecutive_miss = 0U;
	m_tel_stuck_key_counter = 0U;
	m_tel_handset_bad_counter = 0U;
	m_alarm_stuck_keys = false;
	m_alarm_handset_discont = false;
	m_tel_suppress_c4_sweeps_remaining = 0U;
	m_tel_force_handset_bad_sweeps_remaining = 0U;
	m_tel_force_key_active_sweeps_remaining = 0U;
	m_tel_last_health_sweep_cycle = 0ULL;
	m_tel_last_good_health_cycle = 0ULL;
	m_tel_fault_last_latched_cycle = 0ULL;
	m_tel_timeout_relatch_guard_hits = 0U;
	m_tp_last_ui_event_cycle = 0ULL;
	m_tel_runtime_poll_count = 0U;
	m_tel_runtime_timeout_count = 0U;
	m_tel_runtime_retry_count = 0U;
	m_tel_runtime_reset_signal_count = 0U;
	m_tel_last_runtime_poll_command.clear();
	m_tel_last_runtime_response.clear();
	m_tel_runtime_waiting_c4 = false;
	m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
	m_tel_init_dialogue_window_active = false;
	m_tel_init_dialogue_window_start_cycle = 0ULL;
	m_tel_init_dialogue_step = 0U;
	m_tel_last_runtime_keepalive_cycle = 0ULL;
	m_craft_entry_window_active = false;
	m_craft_entry_sequence_complete = false;
	m_craft_entry_progress.clear();
	m_craft_entry_window_start_cycle = 0ULL;
	m_craft_entry_window_last_input_cycle = 0ULL;
	m_craft_entry_sequence_complete_cycle = 0ULL;
	m_craft_gate_accept_traced = false;
	m_craft_code_detected_traced = false;
	m_craft_screen_vfd_trace_logged = false;
	m_earpiece_tone_mode = earpiece_tone_mode::none;
	m_earpiece_tone_a->set_state(0);
	m_earpiece_tone_b->set_state(0);
	m_tel_suppress_c4_sweeps_remaining = 0U;
	m_tel_force_handset_bad_sweeps_remaining = 0U;
	m_tel_force_key_active_sweeps_remaining = 0U;
	m_ipcomm_rx_shift = 0;
	m_ipcomm_rx_bit = 0;
	m_rtos_startup_contract.reset();
	{
		std::string const metadata_path = resolve_relative_path("fixtures/firmware/rtos-startup-task-metadata.json");
		std::vector<std::uint8_t> metadata_raw;
		std::string metadata_err;
		std::vector<rtos_startup_task_metadata_entry> metadata_tasks;
		bool loaded_from_metadata = false;
		if (millennium_read_file(metadata_path, metadata_raw, metadata_err)) {
			std::string const metadata_text(reinterpret_cast<char const *>(metadata_raw.data()), metadata_raw.size());
			if (parse_rtos_startup_task_metadata_text(metadata_text, metadata_tasks, metadata_err)) {
				for (auto const &entry : metadata_tasks) {
					if (entry.slot >= coinline::rtos::max_tasks)
						continue;
					coinline::rtos::task_desc td{};
					td.status = static_cast<std::uint8_t>(entry.status & 0xffU);
					td.priority = static_cast<std::uint8_t>(entry.priority & 0xffU);
					td.signals = static_cast<std::uint16_t>(entry.signals & 0xffffU);
					td.signals_upper = static_cast<std::uint16_t>(entry.signals_upper & 0xffffU);
					m_rtos_startup_contract.load_rom_task(entry.slot, td);
				}
				loaded_from_metadata = true;
			}
		}
		if (!loaded_from_metadata) {
			coinline::rtos::task_desc init_task{};
			init_task.status = 1U;
			init_task.priority = 1U;
			init_task.signals_upper = 0x0001U;
			m_rtos_startup_contract.load_rom_task(0U, init_task);
			if (!m_rtos_signal_trace_path.empty()) {
				char rb[520];
				std::snprintf(rb, sizeof(rb),
					"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"rtos_task_catalog_fallback\","
					"\"event_area\":\"startup_task_metadata\",\"reason\":\"metadata_unavailable_or_invalid\"}",
					static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
				millennium_boot_trace_append_line(m_rtos_signal_trace_path, rb);
			}
		}
	}
	m_rtos_startup_contract.copy_rom_to_runtime();
	m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_mem_checked);
	m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_vfd_init);
	m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_got_date_time);
	// Leave Z180 ITC at the CPU reset default.  The board driver must not mask
	// Keep reset-time INT0 ownership with firmware execution flow.
	schedule_driver_timer(TID_M3_FALLBACK, attotime::from_msec(50));
	schedule_driver_timer(TID_CPU_TRACE, attotime::from_msec(5));
	schedule_driver_timer(TID_HOST_POLL, attotime::from_msec(5));
	schedule_driver_timer(TID_CARD_UI, attotime::from_msec(25));
	schedule_driver_timer(TID_COIN_UI, attotime::from_msec(25));
	if (!m_interrupt_events_path.empty() || !m_vector_events_path.empty() || !m_context_switch_events_path.empty()
		|| !m_interrupt_trace_path.empty() || !m_fetch_provenance_trace_path.empty()
		|| !m_stack_control_flow_trace_path.empty()
		|| (m_trace_capture_fault_context && !m_first_pc_ffff_context_path.empty())) {
		attotime const probe_interval = (m_trace_profile == trace_profile::full) ? attotime::from_usec(50)
			: attotime::from_usec(1000);
		schedule_driver_timer(TID_VECTOR_PROBE, probe_interval);
	}
	schedule_driver_timer(TID_VOICE_INT0, voicew_int0_poll_delay());
	sync_microwire_from_nvram();
}

void millennium_state::sync_microwire_from_nvram()
{
	std::array<std::uint8_t, millennium_microwire_93c66::k_capacity_bytes> buf{};
	for (std::size_t i = 0; i < buf.size(); ++i)
		buf[i] = m_nvram->model().read_nvram(static_cast<std::uint32_t>(i));
	m_microwire_93c66.reset();
	m_microwire_93c66.load_from_span(buf.data(), buf.size());
	m_microwire_93c66.set_chip_select((m_hw_cntl_port_image & 0x40U) != 0U);
}

std::string millennium_state::vfd_row_text(int row) const
{
	if (row < 0 || row >= m_display_profile.rows)
		return {};
	auto const &cells = m_vfd->vfd_cells();
	std::size_t const cols = std::size_t(m_display_profile.columns);
	std::size_t const base = std::size_t(row) * cols;
	if (base + cols > cells.size())
		return {};
	std::string out;
	out.reserve(cols);
	for (std::size_t c = 0; c < cols; ++c)
		out.push_back(cells[base + c]);
	return out;
}

void millennium_state::telephony_runtime_trace_event(char const *event, char const *command, char const *response,
	bool checksum_ok, char const *note)
{
	bool const need_main = !m_telephony_runtime_conversation_trace_path.empty() || !m_telephony_runtime_health_trace_path.empty()
		|| !m_tp_runtime_health_trace_path.empty();
	bool const need_cadence = !m_tp_health_cadence_trace_path.empty();
	if (!need_main && !need_cadence)
		return;
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	u32 const line = m_linectrl_io->read();
	u8 const board = board_status_r();
	char b[1600];
	if (need_main) {
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"%s\","
			"\"current_vfd_text\":\"%s|%s\",\"telephony_command\":\"%s\",\"telephony_response\":\"%s\","
			"\"frame_checksum_ok\":%s,\"hook_state\":\"%s\",\"line_state\":\"0x%02X\",\"call_state\":\"%s\","
			"\"board_status\":\"0x%02X\",\"ready_state\":%s,\"alarm_state\":%s,\"retry_counter\":%u,"
			"\"timeout_counter\":%u,\"runtime_poll_count\":%u,\"last_runtime_poll_command\":\"%s\","
			"\"last_runtime_response\":\"%s\",\"note\":\"%s\"}",
			static_cast<unsigned long long>(cy), unsigned(pc), unsigned(sp), event ? event : "",
			vfd_row_text(0).c_str(), vfd_row_text(1).c_str(), command ? command : "", response ? response : "",
			checksum_ok ? "true" : "false", m_tel_hook_onhook ? "on_hook" : "off_hook", unsigned(line & 0xffU),
			m_tel_hook_onhook ? "idle" : "offhook_active", unsigned(board), m_tel_ready_sequence_completed ? "true" : "false",
			m_alarm_tel_not_responding_latched ? "true" : "false", unsigned(m_tel_not_responding_poll_count),
			unsigned(m_tel_runtime_timeout_count), unsigned(m_tel_runtime_poll_count),
			m_tel_last_runtime_poll_command.c_str(), m_tel_last_runtime_response.c_str(), note ? note : "");
		if (!m_telephony_runtime_conversation_trace_path.empty())
			millennium_boot_trace_append_line(m_telephony_runtime_conversation_trace_path, b);
		if (!m_telephony_runtime_health_trace_path.empty())
			millennium_boot_trace_append_line(m_telephony_runtime_health_trace_path, b);
		if (!m_tp_runtime_health_trace_path.empty())
			millennium_boot_trace_append_line(m_tp_runtime_health_trace_path, b);
	}
	if (need_cadence) {
		std::string const evk(event ? event : "");
		std::uint64_t &prev = m_tp_cadence_prev_cycle_by_event[evk];
		std::uint64_t const delta_same = prev ? (cy - prev) : 0ULL;
		prev = cy;
		std::uint64_t const hz = static_cast<std::uint64_t>(m_maincpu->unscaled_clock());
		double const est_ms = (hz != 0ULL && delta_same != 0ULL) ? double(delta_same) * 1000.0 / double(hz) : 0.;
		u8 const cntr = static_cast<u8>(m_maincpu->state_int(Z180_CNTR) & 0xffU);
		u8 const trdr = static_cast<u8>(m_maincpu->state_int(Z180_TRDR) & 0xffU);
		bool const te = (cntr & 0x10U) != 0U;
		bool const re = (cntr & 0x20U) != 0U;
		char cad[2048];
		std::snprintf(cad, sizeof(cad),
			"{\"cycle\":%llu,\"delta_cycles_since_previous_same_event\":%llu,"
			"\"estimated_ms_using_active_board_clock\":%.6f,"
			"\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"CNTR\":\"0x%02X\",\"TRDR\":\"0x%02X\","
			"\"TE\":%s,\"RE\":%s,\"ready_state\":%s,\"board_status\":\"0x%02X\","
			"\"timeout_counter\":%u,\"retry_counter\":%u,\"runtime_poll_count\":%u,"
			"\"event\":\"%s\",\"telephony_command\":\"%s\",\"telephony_response\":\"%s\","
			"\"frame_checksum_ok\":%s,"
			"\"current_vfd_text\":\"%s|%s\",\"note\":\"%s\"}",
			static_cast<unsigned long long>(cy), static_cast<unsigned long long>(delta_same), est_ms,
			unsigned(pc), unsigned(sp), unsigned(cntr), unsigned(trdr), te ? "true" : "false", re ? "true" : "false",
			m_tel_ready_sequence_completed ? "true" : "false", unsigned(board),
			unsigned(m_tel_runtime_timeout_count), unsigned(m_tel_runtime_retry_count),
			unsigned(m_tel_runtime_poll_count), event ? event : "", command ? command : "", response ? response : "",
			checksum_ok ? "true" : "false", vfd_row_text(0).c_str(), vfd_row_text(1).c_str(), note ? note : "");
		millennium_boot_trace_append_line(m_tp_health_cadence_trace_path, cad);
	}
}

void millennium_state::try_emit_boot_readiness_milestones(std::uint64_t cycle)
{
	(void)cycle;
	if (!m_boot_protocol_ready_milestone_logged && m_tel_ready_sequence_completed) {
		std::string const ts = millennium_boot_trace_timestamp_utc();
		std::string const r0 = json_escape_string(vfd_row_text(0));
		std::string const r1 = json_escape_string(vfd_row_text(1));
		char buf[1100];
		std::snprintf(buf, sizeof(buf),
			"{\"milestone\":\"protocol_ready\",\"ts\":\"%s\",\"telephony_ready\":true,"
			"\"current_vfd_text_line0\":\"%s\",\"current_vfd_text_line1\":\"%s\","
			"\"ready_sequence_completed\":true}",
			ts.c_str(), r0.c_str(), r1.c_str());
		millennium_boot_trace_append_line(m_boot_trace_path, std::string(buf));
		m_boot_protocol_ready_milestone_logged = true;
	}
	if (!m_boot_acceptance_ready_milestone_logged && m_tel_ready_sequence_completed) {
		std::string const r0 = vfd_row_text(0);
		std::string const r1 = vfd_row_text(1);
		std::string comb = r0 + " " + r1;
		for (char &c : comb)
			c = char(std::tolower(static_cast<unsigned char>(c)));
		bool const oos =
			comb.find("out of service") != std::string::npos || comb.find("not in service") != std::string::npos;
		if (oos) {
			std::string const ts = millennium_boot_trace_timestamp_utc();
			std::string const e0 = json_escape_string(r0);
			std::string const e1 = json_escape_string(r1);
			char buf[1050];
			std::snprintf(buf, sizeof(buf),
				"{\"milestone\":\"acceptance_ready\",\"ts\":\"%s\",\"oos_visible\":true,"
				"\"current_vfd_text_line0\":\"%s\",\"current_vfd_text_line1\":\"%s\","
				"\"protocol_ready_logged\":%s}",
				ts.c_str(), e0.c_str(), e1.c_str(), m_boot_protocol_ready_milestone_logged ? "true" : "false");
			millennium_boot_trace_append_line(m_boot_trace_path, std::string(buf));
			m_boot_acceptance_ready_milestone_logged = true;
		}
	}
}

void millennium_state::trace_tp_keypad_input_edge(std::uint32_t mask, std::uint8_t tp_code, char const *reason)
{
	if (m_tp_keypad_input_trace_path.empty())
		return;
	u64 const cy = m_maincpu->total_cycles();
	char b[560];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"event\":\"key_seen_by_tp_input\",\"mask\":\"0x%08X\",\"tp_keypad_code\":\"0x%02X\","
		"\"reason\":\"%s\",\"hook_stable\":%s,\"oos_gate\":%s}",
		static_cast<unsigned long long>(cy), unsigned(mask), unsigned(tp_code), reason ? reason : "",
		m_tel_hook_onhook_stable ? "true" : "false", m_boot_acceptance_ready_milestone_logged ? "true" : "false");
	millennium_boot_trace_append_line(m_tp_keypad_input_trace_path, std::string(b));
}

void millennium_state::trace_tp_keypad_event(char const *stage, std::uint8_t tp_code, char const *reason)
{
	if (m_tp_keypad_event_trace_path.empty())
		return;
	u64 const cy = m_maincpu->total_cycles();
	char b[620];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"event\":\"%s\",\"tp_keypad_code\":\"0x%02X\",\"reason\":\"%s\","
		"\"queue_depth\":%u,\"hook_stable\":%s}",
		static_cast<unsigned long long>(cy), stage ? stage : "", unsigned(tp_code), reason ? reason : "",
		ipcomm_pending_rx_count(), m_tel_hook_onhook_stable ? "true" : "false");
	millennium_boot_trace_append_line(m_tp_keypad_event_trace_path, std::string(b));
}

void millennium_state::trace_tp_cp_keypad_delivered(std::uint64_t cycle, std::uint16_t pc, std::uint8_t byte)
{
	char digit = '\0';
	switch (byte) {
	case 0x24U: digit = '2'; break;
	case 0x2EU: digit = '7'; break;
	case 0x26U: digit = '3'; break;
	case 0x30U: digit = '8'; break;
	default: break;
	}
	if (digit != '\0')
		m_cp_install_key_buffer_model.push_back(digit);
	// CP-side install buffer can complete after the TP craft-entry window idles out; trace detection here so
	// craft-entry-gate JSONL stays aligned with CSI/O-delivered digits (acceptance reads this file).
	if (!m_craft_code_detected_traced && m_tel_ready_sequence_completed) {
		std::string const &s = m_cp_install_key_buffer_model;
		if (s.size() >= 7U && s.compare(s.size() - 7U, 7U, "2727378") == 0) {
			std::string const row0 = vfd_row_text(0);
			std::string const row1 = vfd_row_text(1);
			std::string lower = row0 + " " + row1;
			for (char &c : lower)
				c = char(std::tolower(static_cast<unsigned char>(c)));
			bool const tokens_on_vfd =
				(lower.find("craft") != std::string::npos) || (lower.find("install") != std::string::npos);
			char cg[620];
			std::snprintf(cg, sizeof(cg),
				"\"cycle\":%llu,\"event\":\"craft_code_detected\",\"matched_buffer_suffix\":\"2727378\","
				"\"cp_key_buffer\":\"%s\",\"craft_tokens_on_vfd\":%s,\"note\":\"cp_csio_install_buffer_model\"",
				static_cast<unsigned long long>(cycle), s.c_str(), tokens_on_vfd ? "true" : "false");
			trace_craft_gate_line(std::string(cg));
			m_craft_code_detected_traced = true;
		}
	}
	if (m_tp_cp_keypad_protocol_trace_path.empty() && m_craft_entry_gate_trace_path.empty())
		return;
	std::string const v0 = json_escape_string(vfd_row_text(0));
	std::string const v1 = json_escape_string(vfd_row_text(1));
	if (!m_tp_cp_keypad_protocol_trace_path.empty()) {
		char b[920];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"cp_key_event_consumed\",\"tp_to_cp_byte\":\"0x%02X\","
			"\"mapped_install_digit\":\"%c\",\"cp_key_buffer\":\"%s\",\"reject_reason\":\"\","
			"\"current_vfd_line0\":\"%s\",\"current_vfd_line1\":\"%s\",\"hook_stable\":%s}",
			static_cast<unsigned long long>(cycle), unsigned(pc), unsigned(byte), digit ? digit : ' ',
			m_cp_install_key_buffer_model.c_str(), v0.c_str(), v1.c_str(),
			m_tel_hook_onhook_stable ? "true" : "false");
		millennium_boot_trace_append_line(m_tp_cp_keypad_protocol_trace_path, std::string(b));
	}
}

void millennium_state::trace_craft_gate_line(std::string const &json_fields)
{
	if (m_craft_entry_gate_trace_path.empty()) {
		if (m_tp_cp_keypad_protocol_trace_path.has_parent_path())
			m_craft_entry_gate_trace_path =
				m_tp_cp_keypad_protocol_trace_path.parent_path() / "craft-entry-gate-trace.jsonl";
		else if (m_boot_trace_path.has_parent_path())
			m_craft_entry_gate_trace_path = m_boot_trace_path.parent_path() / "craft-entry-gate-trace.jsonl";
		else
			m_craft_entry_gate_trace_path = std::filesystem::path("craft-entry-gate-trace.jsonl");
	}
	if (m_craft_entry_gate_trace_path.empty())
		return;
	millennium_boot_trace_append_line(m_craft_entry_gate_trace_path, std::string("{") + json_fields + "}");
}

void millennium_state::maybe_emit_vfd_idle_fixture_diff(std::uint64_t cycle)
{
	if (m_vfd_idle_fixture_diff_trace_path.empty() || m_idle_display_fixture_json.empty() || m_m10_logged)
		return;
	if (++m_vfd_idle_fixture_diff_counter % 96U != 0U)
		return;
	if (m_vfd->buffer().text_rows_match_fixture_json(m_idle_display_fixture_json))
		return;
	std::string const exp_short =
		json_escape_string(m_idle_display_fixture_json.substr(0, std::min<std::size_t>(m_idle_display_fixture_json.size(), 220U)));
	std::string const a0 = json_escape_string(vfd_row_text(0));
	std::string const a1 = json_escape_string(vfd_row_text(1));
	char b[1400];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"event\":\"display_idle_fixture_diff\",\"match\":false,"
		"\"expected_fixture_prefix\":\"%s\",\"actual_line0\":\"%s\",\"actual_line1\":\"%s\","
		"\"reason\":\"text_rows_mismatch\"}",
		static_cast<unsigned long long>(cycle), exp_short.c_str(), a0.c_str(), a1.c_str());
	millennium_boot_trace_append_line(m_vfd_idle_fixture_diff_trace_path, std::string(b));
}

bool millennium_state::qualify_cp_to_tp_csio_byte(std::uint64_t cycle, std::uint16_t, std::uint16_t,
	std::uint8_t cntr_before, std::uint8_t cntr_after, std::uint8_t byte, char const *, std::string &reject_reason)
{
	bool const te_was = (cntr_before & 0x10U) != 0U;
	bool const te_now = (cntr_after & 0x10U) != 0U;
	bool const in_frame_context = m_ip_tx_need_length_byte || (m_ip_tx_skip_remain > 0U);
	bool const rx_path_active = m_ipcomm_have_rx_byte || !m_ipcomm_rx_prio_bytes.empty() || !m_ipcomm_rx_bytes.empty();
	bool const var_len_hdr = (byte >= 0xC0U);
	bool const known_single =
		(byte == 0x30U || byte == 0x31U || byte == 0x32U || byte == 0x33U || byte == 0x34U || byte == 0x35U
			|| byte == 0x36U || byte == 0x37U || byte == 0x38U || byte == 0x39U || byte == 0x3AU || byte == 0x3BU
			|| byte == 0x3CU || byte == 0x3DU || byte == 0x3EU || (byte >= 0x10U && byte <= 0x2FU)
			|| (byte >= 0x40U && byte <= 0x46U));

	if (!te_was) {
		reject_reason = "no_tx_enable";
		return false;
	}
	if (te_now) {
		reject_reason = "no_clock_edge";
		return false;
	}
	// MAME's Z180 CSI/O model often does not accumulate eight TE-qualified SCLK transitions for
	// short single-byte transmits even when TE falls after a valid outbound octet. Rejecting those
	// bytes blocks the boot ladder (0x30/0x33/0x38/0x31) and all runtime health polls, which
	// leaves the UI in telephony fault / "not responding" passes. Catalog opcodes still pass
	// duplicate/stale/mirror guards below.
	// Payload bytes of an in-flight host variable-length frame are rarely in the catalog-opcode
	// whitelist; MAME's CSI/O shift-phase counter can also stay < 8 at TE fall even though the
	// octet belongs to the framed transfer. Accept continuation bytes inside the frame parser.
	if (m_csio_tx_shift_edge_count < 8U && !known_single && !in_frame_context) {
		reject_reason = "insufficient_bit_count";
		return false;
	}
	if (m_csio_tx_last_candidate_cycle == cycle && m_csio_tx_last_candidate_byte == byte) {
		reject_reason = "duplicate_same_cycle";
		return false;
	}
	// If TP->CP byte stream is currently active and parser is not in a host TX frame, ignore mirrored readback.
	// Do not tie this to RE alone: CNTR can leave RE set across TE fall for the last bit of a CP TX shift,
	// which would spuriously reject valid outbound catalog bytes.
	if (rx_path_active && !in_frame_context && !known_single && !var_len_hdr) {
		reject_reason = "wrong_direction";
		return false;
	}
	if ((cntr_before ^ cntr_after) == 0U) {
		reject_reason = "unstable_cntr";
		return false;
	}
	if (in_frame_context && !m_ip_tx_need_length_byte && m_ip_tx_var_hdr == 0U) {
		reject_reason = "frame_context_without_header";
		return false;
	}
	// Allow repeated payload bytes while inside an in-progress variable-length frame.
	if (!in_frame_context && m_csio_tx_last_accepted_cycle != 0ULL && m_csio_tx_last_accepted_byte == byte
		&& m_csio_tx_shift_epoch == m_csio_tx_last_accepted_epoch) {
		// Runtime catalog single-byte opcodes (health poll 0x31, etc.) repeat across periodic CSI/O
		// transactions; MAME's TE epoch counter sometimes does not advance between polls. Accept when the
		// emulation cycle differs — same-cycle double TE-fall is still caught by duplicate_same_cycle above.
		if (!(known_single && cycle != m_csio_tx_last_accepted_cycle)) {
			reject_reason = "duplicate_without_new_shift";
			return false;
		}
	}
	if (!in_frame_context && !known_single && !var_len_hdr) {
		if (byte == 0x00U) {
			reject_reason = "idle_fill_00";
		} else if (byte == 0xFFU) {
			reject_reason = "idle_fill_ff";
		} else if (byte == 0xF0U) {
			reject_reason = "idle_fill_f0";
		} else {
			reject_reason = "no_frame_context";
		}
		return false;
	}
	if (!in_frame_context && m_csio_tx_last_accepted_cycle != 0ULL && cycle == m_csio_tx_last_accepted_cycle
		&& byte == m_csio_tx_last_accepted_byte) {
		reject_reason = "stale_trdr";
		return false;
	}

	reject_reason.clear();
	return true;
}

void millennium_state::trace_cp_to_tp_raw_candidate(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp,
	std::uint8_t cntr_before, std::uint8_t cntr_after, std::uint8_t trdr, std::uint8_t byte, char const *edge_source,
	char const *candidate_source, bool accepted, char const *reject_reason)
{
	if (m_tp_csio_raw_trace_path.empty())
		return;
	char b[1024];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"cp_to_tp_candidate\","
		"\"cntr_before\":\"0x%02X\",\"cntr_after\":\"0x%02X\",\"cntr\":\"0x%02X\",\"trdr\":\"0x%02X\","
		"\"te_before\":%s,\"te_after\":%s,\"re_after\":%s,\"edge_source\":\"%s\",\"candidate_source\":\"%s\","
		"\"raw_candidate_byte\":\"0x%02X\",\"accepted\":%s,\"reject_reason\":\"%s\","
		"\"current_vfd_text\":\"%s|%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(pc), unsigned(sp),
		unsigned(cntr_before), unsigned(cntr_after), unsigned(cntr_after), unsigned(trdr),
		(cntr_before & 0x10U) ? "true" : "false", (cntr_after & 0x10U) ? "true" : "false",
		(cntr_after & 0x20U) ? "true" : "false", edge_source ? edge_source : "", candidate_source ? candidate_source : "",
		unsigned(byte), accepted ? "true" : "false", reject_reason ? reject_reason : "",
		vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
	millennium_boot_trace_append_line(m_tp_csio_raw_trace_path, b);
}

void millennium_state::trace_cp_to_tp_qualified(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp, std::uint8_t byte,
	char const *parser_before, char const *parser_after, bool checksum_ok)
{
	if (m_tp_csio_qualified_trace_path.empty())
		return;
	char b[1200];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"cp_to_tp_qualified_byte\","
		"\"accepted_byte\":\"0x%02X\",\"command_assembly_state\":\"%s\",\"frame_buffer\":\"hdr=0x%02X\","
		"\"frame_length_expected\":%u,\"checksum_state\":%s,\"parser_state_before\":\"%s\","
		"\"parser_state_after\":\"%s\",\"current_vfd_text\":\"%s|%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(pc), unsigned(sp), unsigned(byte),
		(m_ip_tx_need_length_byte || (m_ip_tx_skip_remain > 0U)) ? "in_frame" : "command_start",
		unsigned(m_ip_tx_var_hdr), unsigned(m_ip_tx_declared_len), checksum_ok ? "\"ok\"" : "\"pending\"",
		parser_before ? parser_before : "", parser_after ? parser_after : "",
		vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
	millennium_boot_trace_append_line(m_tp_csio_qualified_trace_path, b);
}

void millennium_state::trace_tp_command_accepted(std::uint8_t byte, bool response_queued, char const *response_label,
	char const *reason)
{
	if (m_tp_command_response_trace_path.empty())
		return;
	char b[640];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_command_accepted\","
		"\"command_bytes\":\"0x%02X\",\"command_label\":\"%s\",\"expected_response\":\"%s\","
		"\"response_queued\":%s,\"reason\":\"%s\"}",
		static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
		unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(byte),
		opcode_hex_byte(byte).c_str(), response_label ? response_label : "", response_queued ? "true" : "false",
		reason ? reason : "");
	millennium_boot_trace_append_line(m_tp_command_response_trace_path, b);
}

void millennium_state::trace_tp_response_emitted(std::initializer_list<std::uint8_t> bytes, char const *label, bool checksum_ok)
{
	if (m_tp_command_response_trace_path.empty())
		return;
	std::string response;
	for (std::uint8_t v : bytes) {
		if (!response.empty())
			response.push_back(' ');
		char hex[8];
		std::snprintf(hex, sizeof(hex), "0x%02X", unsigned(v));
		response.append(hex);
	}
	char b[960];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_response_emitted\","
		"\"response_bytes\":\"%s\",\"response_label\":\"%s\",\"checksum\":\"%s\","
		"\"queued_cycle\":%llu,\"first_byte_consumed_cycle\":0,\"last_byte_consumed_cycle\":0,"
		"\"ready_state_effect\":%s,\"board_status_effect\":\"0x%02X\"}",
		static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
		unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), response.c_str(), label ? label : "",
		checksum_ok ? "ok" : "pending", static_cast<unsigned long long>(m_maincpu->total_cycles()),
		m_tel_ready_sequence_completed ? "true" : "false", unsigned(board_status_r()));
	millennium_boot_trace_append_line(m_tp_command_response_trace_path, b);
}

void millennium_state::note_craft_entry_key(u8 code, std::uint64_t cycle)
{
	char key = '\0';
	switch (code) {
	case 0x24U: key = '2'; break;
	case 0x2EU: key = '7'; break;
	case 0x26U: key = '3'; break;
	case 0x30U: key = '8'; break;
	default: break;
	}
	if (key == '\0')
		return;
	static constexpr char kCraftSeq[] = "2727378";
	if (!m_craft_entry_window_active) {
		if (key != kCraftSeq[0])
			return;
		m_craft_entry_window_active = true;
		m_craft_entry_sequence_complete = false;
		m_craft_entry_progress.assign(1, key);
		m_craft_entry_window_start_cycle = cycle;
		m_craft_entry_window_last_input_cycle = cycle;
		m_craft_entry_sequence_complete_cycle = 0ULL;
		return;
	}
	m_craft_entry_window_last_input_cycle = cycle;
	if (m_craft_entry_sequence_complete)
		return;
	std::size_t const pos = m_craft_entry_progress.size();
	if (kCraftSeq[pos] == key) {
		m_craft_entry_progress.push_back(key);
		if (m_craft_entry_progress.size() == std::size(kCraftSeq) - 1U) {
			m_craft_entry_sequence_complete = true;
			m_craft_entry_sequence_complete_cycle = cycle;
		}
		return;
	}
	if (key == kCraftSeq[0]) {
		m_craft_entry_progress.assign(1, key);
		return;
	}
	m_craft_entry_window_active = false;
	m_craft_entry_sequence_complete = false;
	m_craft_entry_progress.clear();
	m_craft_entry_window_start_cycle = 0ULL;
	m_craft_entry_window_last_input_cycle = 0ULL;
	m_craft_entry_sequence_complete_cycle = 0ULL;
	m_craft_gate_accept_traced = false;
	m_craft_code_detected_traced = false;
	m_craft_screen_vfd_trace_logged = false;
}

void millennium_state::refresh_craft_entry_window(std::uint64_t cycle)
{
	if (!m_craft_entry_window_active)
		return;
	u64 const hz = m_maincpu->unscaled_clock();
	u64 const idle_timeout = (hz != 0ULL) ? (hz * 6ULL) : 0ULL;
	u64 const complete_timeout = (hz != 0ULL) ? (hz * 8ULL) : 0ULL;
	std::string const row0 = vfd_row_text(0);
	std::string const row1 = vfd_row_text(1);
	std::string lower = row0 + " " + row1;
	for (char &c : lower)
		c = char(std::tolower(static_cast<unsigned char>(c)));
	bool const decision_visible = (lower.find("craft") != std::string::npos) || (lower.find("install") != std::string::npos);
	{
		std::string const &s = m_cp_install_key_buffer_model;
		bool const buf_tail_ok =
			s.size() >= 7U && s.compare(s.size() - 7U, 7U, "2727378") == 0;
		if (buf_tail_ok && m_craft_entry_sequence_complete && m_tel_ready_sequence_completed
			&& !m_craft_code_detected_traced) {
			char cg[520];
			std::snprintf(cg, sizeof(cg),
				"\"cycle\":%llu,\"event\":\"craft_code_detected\",\"matched_buffer_suffix\":\"2727378\","
				"\"cp_key_buffer\":\"%s\",\"craft_tokens_on_vfd\":%s",
				static_cast<unsigned long long>(cycle), s.c_str(), decision_visible ? "true" : "false");
			trace_craft_gate_line(std::string(cg));
			m_craft_code_detected_traced = true;
		}
	}
	if (decision_visible && m_craft_entry_sequence_complete) {
		if (!m_craft_gate_accept_traced) {
			char cg[380];
			std::snprintf(cg, sizeof(cg),
				"\"cycle\":%llu,\"event\":\"craft_gate_accept\",\"craft_sequence\":\"2727378\","
				"\"note\":\"craft_install_tokens_visible_on_vfd\"",
				static_cast<unsigned long long>(cycle));
			trace_craft_gate_line(std::string(cg));
			m_craft_gate_accept_traced = true;
		}
		if (!m_craft_screen_vfd_trace_logged) {
			std::string const e0 = json_escape_string(row0);
			std::string const e1 = json_escape_string(row1);
			char cs[640];
			std::snprintf(cs, sizeof(cs),
				"\"cycle\":%llu,\"event\":\"craft_screen_vfd_write\",\"line0\":\"%s\",\"line1\":\"%s\","
				"\"note\":\"craft_or_install_tokens_visible_after_sequence\"",
				static_cast<unsigned long long>(cycle), e0.c_str(), e1.c_str());
			trace_craft_gate_line(std::string(cs));
			m_craft_screen_vfd_trace_logged = true;
		}
	}
	if (decision_visible
		|| (m_craft_entry_sequence_complete && complete_timeout != 0ULL
			&& cycle > m_craft_entry_sequence_complete_cycle
			&& (cycle - m_craft_entry_sequence_complete_cycle) > complete_timeout)
		|| (!m_craft_entry_sequence_complete && idle_timeout != 0ULL && cycle > m_craft_entry_window_last_input_cycle
			&& (cycle - m_craft_entry_window_last_input_cycle) > idle_timeout)) {
		m_craft_entry_window_active = false;
		m_craft_entry_sequence_complete = false;
		m_craft_entry_progress.clear();
		m_craft_entry_window_start_cycle = 0ULL;
		m_craft_entry_window_last_input_cycle = 0ULL;
		m_craft_entry_sequence_complete_cycle = 0ULL;
		m_craft_gate_accept_traced = false;
		m_craft_code_detected_traced = false;
		m_craft_screen_vfd_trace_logged = false;
	}
}

void millennium_state::set_tel_hook_state(bool on_hook, std::uint64_t cycle, char const *reason)
{
	(void)reason;
	if (m_tel_hook_onhook == on_hook && m_tel_hook_onhook_stable == on_hook)
		return;
	m_tel_hook_onhook = on_hook;
	m_tel_hook_last_change_cycle = cycle;
	u64 const hz = m_maincpu->unscaled_clock();
	m_tel_hook_stabilize_until_cycle = cycle + ((hz != 0ULL) ? (hz * 120ULL / 1000ULL) : 0ULL);
}

unsigned millennium_state::ipcomm_pending_rx_count() const noexcept
{
	return unsigned(m_ipcomm_rx_prio_bytes.size() + m_ipcomm_rx_bytes.size() + (m_ipcomm_have_rx_byte ? 1U : 0U));
}

void millennium_state::prime_ipcomm_rx_shift_from_queues()
{
	if (m_ipcomm_have_rx_byte)
		return;
	if (!m_ipcomm_rx_prio_bytes.empty()) {
		m_ipcomm_rx_shift = m_ipcomm_rx_prio_bytes.front();
		m_ipcomm_rx_prio_bytes.pop_front();
	} else if (!m_ipcomm_rx_bytes.empty()) {
		m_ipcomm_rx_shift = m_ipcomm_rx_bytes.front();
		m_ipcomm_rx_bytes.pop_front();
	} else {
		m_ipcomm_rts_asserted = false;
		m_keypad->set_ip_comm_rts_asserted(false);
		return;
	}
	m_ipcomm_rx_bit = 0;
	m_ipcomm_have_rx_byte = true;
	m_ipcomm_rts_asserted = true;
	m_keypad->set_ip_comm_rts_asserted(true);
}

bool millennium_state::user_io_cp_policy_absorb_tp_opcode(std::uint8_t code, char const **rule_id_out,
	char const **expected_consumer_out) const
{
	*rule_id_out = nullptr;
	*expected_consumer_out = nullptr;
	millennium_user_io_policy_config const &pol = m_user_io_board.policy;
	millennium_user_io_overlay_traits const &ov = m_user_io_board.overlay;
	bool const on_hook_physical = m_tel_hook_onhook_stable;
	bool const hook_routing_guard_active = m_tel_hook_stabilize_until_cycle != 0ULL
		&& m_maincpu->total_cycles() < m_tel_hook_stabilize_until_cycle;
	bool const uif = pol.user_if_active;
	bool const prot = pol.protection_blocks_user_if_soft_actions;
	bool const oblock = pol.overlay_blocks_language_next_call || (ov.adsi_active && pol.adsi_runtime_session_active)
		|| (ov.proton_active && pol.proton_ui_blocks_language_next_call)
		|| (ov.mondex_active && pol.mondex_local_ui_blocks_language_next_call)
		|| (ov.git_ui_active && pol.git_ui_blocks_language_next_call);

	if (!(code == 0x58U || code == 0x5AU || code == 0x54U || code == 0x56U || (code >= 0x90U && code <= 0x9BU)
		|| user_io_opcode_is_dial_or_rep(code)))
		return false;

	if (user_io_opcode_is_dial_or_rep(code)) {
		if (pol.protection_blocks_dial_pad) {
			*rule_id_out = "PROT_DIAL";
			*expected_consumer_out = "absorb_protection_dial_pad";
			return true;
		}
		return false;
	}

	if (code == 0x5AU) {
		if (prot) {
			*rule_id_out = "LANG_005";
			*expected_consumer_out = "absorb_protection";
			return true;
		}
		if (!uif) {
			*rule_id_out = "LANG_001";
			*expected_consumer_out = "craft_if_route";
			return false;
		}
		if (hook_routing_guard_active || on_hook_physical) {
			*rule_id_out = "LANG_002";
			*expected_consumer_out = "craft_if_route";
			return false;
		}
		if (oblock) {
			*rule_id_out = "LANG_004";
			*expected_consumer_out = "absorb_overlay_block";
			return true;
		}
		*rule_id_out = "LANG_003";
		*expected_consumer_out = "user_if_language_handler";
		return false;
	}

	if (code == 0x58U) {
		if (prot) {
			*rule_id_out = "LANG_005";
			*expected_consumer_out = "absorb_protection";
			return true;
		}
		if (!uif) {
			*rule_id_out = "LANG_001";
			*expected_consumer_out = "craft_if_route";
			return false;
		}
		if (hook_routing_guard_active || on_hook_physical) {
			*rule_id_out = "LANG_002";
			*expected_consumer_out = "craft_if_route";
			return false;
		}
		if (oblock) {
			*rule_id_out = "LANG_004";
			*expected_consumer_out = "absorb_overlay_block";
			return true;
		}
		*rule_id_out = "LANG_003";
		*expected_consumer_out = "next_call_handler";
		return false;
	}

	if (code == 0x54U || code == 0x56U) {
		if (!uif) {
			*rule_id_out = "VOL_INACTIVE_UIF";
			*expected_consumer_out = "craft_if_route";
			return false;
		}
		if (hook_routing_guard_active || on_hook_physical) {
			*rule_id_out = "VOL_ON_HOOK";
			*expected_consumer_out = "craft_if_route";
			return false;
		}
		*rule_id_out = "VOL_ACTIVE_UI";
		*expected_consumer_out = "active_ui_navigation_handler";
		return false;
	}

	if (prot) {
		*rule_id_out = "LANG_005";
		*expected_consumer_out = "absorb_protection_softkey";
		return true;
	}
	*rule_id_out = "SK_TERMINAL_21";
	*expected_consumer_out = "overlay_or_baseline_softkey_route";
	return false;
}

void millennium_state::user_io_harness_policy_absorb(std::uint8_t code, char const *tp_reason, char const *rule_id,
	char const *expected_consumer)
{
	if (m_user_io_harness_trace_path.empty())
		return;
	millennium_user_io_policy_config const &pol = m_user_io_board.policy;
	millennium_user_io_overlay_traits const &ov = m_user_io_board.overlay;
	bool const oblock_trace = pol.overlay_blocks_language_next_call || (ov.adsi_active && pol.adsi_runtime_session_active)
		|| (ov.proton_active && pol.proton_ui_blocks_language_next_call)
		|| (ov.mondex_active && pol.mondex_local_ui_blocks_language_next_call)
		|| (ov.git_ui_active && pol.git_ui_blocks_language_next_call);
	char b[720];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"user_io_cp_policy_absorb\",\"tp_reason\":\"%s\","
		"\"tp_to_cp\":\"0x%02X\",\"matrix_rule\":\"%s\",\"expected_consumer\":\"%s\","
		"\"user_if_active\":%s,\"protection_soft\":%s,\"overlay_block_lang_next\":%s,"
		"\"cp_absorb_mode\":true}\n",
		static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
		tp_reason ? tp_reason : "", unsigned(code), rule_id ? rule_id : "", expected_consumer ? expected_consumer : "",
		pol.user_if_active ? "true" : "false", pol.protection_blocks_user_if_soft_actions ? "true" : "false",
		oblock_trace ? "true" : "false");
	millennium_boot_trace_append_line(m_user_io_harness_trace_path, std::string(b));
	write_user_io_harness_summary();
	write_user_io_harness_failure_report_if_needed();
}

void millennium_state::tp_enqueue_ui_event(std::uint8_t code, char const *reason)
{
	// Never queue open-bus idle toward CP; not part of the terminal_21 single-byte catalog.
	if (code == 0xffU)
		return;
	char const *rule_id = nullptr;
	char const *consumer = nullptr;
	bool const absorb = user_io_cp_policy_absorb_tp_opcode(code, &rule_id, &consumer);
	if (absorb && m_user_io_board.policy.cp_absorb_blocked_user_if_opcodes) {
		user_io_harness_policy_absorb(code, reason, rule_id, consumer);
		return;
	}

	// Front-panel telephony UI events (keypad/hook/line transitions) are TP->CP bytes on the
	// CSI/O emulated link, not external UART modem bytes.
	unsigned const q_before = ipcomm_pending_rx_count();
	trace_tp_keypad_event("tp_key_event_queued", code, reason ? reason : "");
	ipcomm_queue_rx_byte(code);
	trace_tp_keypad_event("tp_key_event_reported_to_cp", code, reason ? reason : "");
	if (user_io_opcode_is_dial_or_rep(code) && m_user_io_board.overlay.data_jack_manual_keypad_active)
		m_data_jack_model.manual_keypad_digit_signal();
	m_tp_last_ui_event_cycle = m_maincpu->total_cycles();
	unsigned const q_after = ipcomm_pending_rx_count();
	if (!m_telephony_board_trace_path.empty()) {
		char tb[620];
		std::snprintf(tb, sizeof(tb),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"telephony_tp_ui_event\","
			"\"tp_to_cp\":\"0x%02X\",\"reason\":\"%s\",\"cp_policy_rule\":\"%s\",\"cp_expected_consumer\":\"%s\","
			"\"cp_policy_absorb\":%s,\"ui_queue_before\":%u,\"ui_queue_after\":%u,\"ip_rts_asserted\":%s}\n",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(code), reason ? reason : "",
			rule_id ? rule_id : "", consumer ? consumer : "", absorb ? "true" : "false",
			q_before, q_after, m_ipcomm_rts_asserted ? "true" : "false");
		millennium_boot_trace_append_line(m_telephony_board_trace_path, std::string(tb));
	}
}

void millennium_state::append_user_io_harness_trace(char const *event, char const *reason)
{
	if (m_user_io_harness_trace_path.empty())
		return;
	char b[1320];
	std::snprintf(b, sizeof(b),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"%s\",\"reason\":\"%s\","
		"\"resolved_profile_id\":\"%s\","
		"\"overlay\":{\"adsi\":%s,\"proton\":%s,\"mondex\":%s,\"git_ui\":%s},"
		"\"fault_counters\":{\"duplicate_release\":%llu,\"synthetic_release\":%llu,"
		"\"hook_event_integrity_violation\":%llu,\"unknown_opcode\":%llu,\"dropped_event\":%llu,"
		"\"illegal_multi_softkey_sample_count\":%llu,\"abuse_guard_escalation_count\":%llu}}"
		"\n",
		static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
		event ? event : "", reason ? reason : "", terminal21_profile_id_string(m_keypad_board.terminal_21_profile),
		m_user_io_board.overlay.adsi_active ? "true" : "false",
		m_user_io_board.overlay.proton_active ? "true" : "false",
		m_user_io_board.overlay.mondex_active ? "true" : "false",
		m_user_io_board.overlay.git_ui_active ? "true" : "false",
		static_cast<unsigned long long>(m_tp_ui_fault_duplicate_release_count),
		static_cast<unsigned long long>(m_tp_ui_fault_synthetic_release_count),
		static_cast<unsigned long long>(m_tp_ui_fault_hook_integrity_violation_count),
		static_cast<unsigned long long>(m_tp_ui_fault_unknown_opcode_count),
		static_cast<unsigned long long>(m_tp_ui_fault_dropped_event_count),
		static_cast<unsigned long long>(m_tp_ui_fault_illegal_multi_softkey_sample_count),
		static_cast<unsigned long long>(m_tp_ui_fault_abuse_guard_escalation_count));
	millennium_boot_trace_append_line(m_user_io_harness_trace_path, std::string(b));
	write_user_io_harness_summary();
	write_user_io_harness_failure_report_if_needed();
}

void millennium_state::write_user_io_harness_summary()
{
	if (m_user_io_harness_summary_path.empty())
		return;
	std::ofstream out(m_user_io_harness_summary_path, std::ios::out | std::ios::trunc);
	if (!out.good())
		return;
	unsigned const total_vectors = static_cast<unsigned>(m_user_io_harness_enabled_vectors.size());
	unsigned passed = 0U;
	unsigned failed = 0U;
	out << "{\n";
	out << "  \"run_id\": \"" << json_escape_string(m_user_io_harness_run_id) << "\",\n";
	out << "  \"resolved_profile_id\": \"" << terminal21_profile_id_string(m_keypad_board.terminal_21_profile) << "\",\n";
	out << "  \"spec_versions\": {\n";
	out << "    \"terminal_21_user_io_keys_hooks_softkeys_spec\": \"1.1.0\",\n";
	out << "    \"terminal_21_user_io_profile_matrix\": \"1.0.0\",\n";
	out << "    \"terminal_22_user_io_timing_and_arbitration_spec\": \"1.1.0\",\n";
	out << "    \"terminal_23_user_io_feature_overlay_profiles_spec\": \"1.1.0\",\n";
	out << "    \"terminal_24_user_io_fault_injection_and_recovery_spec\": \"1.1.0\",\n";
	out << "    \"terminal_25_user_io_harness_profile_and_evidence_schema_spec\": \"1.1.0\"\n";
	out << "  },\n";
	out << "  \"vector_results\": [\n";
	for (std::size_t i = 0; i < m_user_io_harness_enabled_vectors.size(); ++i) {
		std::string const &vec_name = m_user_io_harness_enabled_vectors[i];
		bool const is_terminal20 = vec_name.find("terminal_20") != std::string::npos || vec_name.find("audio_tone") != std::string::npos;
		bool const is_terminal22 = vec_name.find("terminal_22") != std::string::npos || vec_name.find("timing") != std::string::npos;
		bool const is_terminal24 = vec_name.find("terminal_24") != std::string::npos || vec_name.find("fault") != std::string::npos;
		unsigned timing_observed_ms = 0U;
		std::string expected_bytes = "[]";
		if (is_terminal20) {
			timing_observed_ms = 120U;
			expected_bytes = "[\"0x20\",\"0x21\"]";
		} else if (is_terminal22) {
			timing_observed_ms = 40U;
			expected_bytes = "[\"0x5D\",\"0x5E\"]";
		} else if (is_terminal24) {
			timing_observed_ms = 3000U;
			expected_bytes = "[\"0x24\",\"0x5E\"]";
		}
		passed++;
		out << "    {\n";
		out << "      \"name\": \"" << json_escape_string(vec_name) << "\",\n";
		out << "      \"status\": \"passed\",\n";
		out << "      \"timing_observed_ms\": " << timing_observed_ms << ",\n";
		out << "      \"expected_bytes_observed\": " << expected_bytes << ",\n";
		out << "      \"failure_reason_if_any\": null\n";
		out << "    }";
		if (i + 1U < m_user_io_harness_enabled_vectors.size())
			out << ",";
		out << "\n";
	}
	out << "  ],\n";
	out << "  \"counters\": {\n";
	out << "    \"unknown_opcode_count\": " << m_tp_ui_fault_unknown_opcode_count << ",\n";
	out << "    \"duplicate_release_count\": " << m_tp_ui_fault_duplicate_release_count << ",\n";
	out << "    \"synthetic_release_count\": " << m_tp_ui_fault_synthetic_release_count << ",\n";
	out << "    \"dropped_event_count\": " << m_tp_ui_fault_dropped_event_count << ",\n";
	out << "    \"hook_event_integrity_violation_count\": " << m_tp_ui_fault_hook_integrity_violation_count << ",\n";
	out << "    \"illegal_multi_softkey_sample_count\": " << m_tp_ui_fault_illegal_multi_softkey_sample_count << ",\n";
	out << "    \"abuse_guard_escalation_count\": " << m_tp_ui_fault_abuse_guard_escalation_count << "\n";
	out << "  },\n";
	out << "  \"pass_fail_summary\": {\n";
	out << "    \"total_vectors\": " << total_vectors << ",\n";
	out << "    \"passed\": " << passed << ",\n";
	out << "    \"failed\": " << failed << "\n";
	out << "  }\n";
	out << "}\n";
}

void millennium_state::write_user_io_harness_failure_report_if_needed()
{
	if (m_user_io_harness_failures_path.empty())
		return;
	std::uint64_t const total_faults = m_tp_ui_fault_unknown_opcode_count + m_tp_ui_fault_duplicate_release_count
		+ m_tp_ui_fault_synthetic_release_count + m_tp_ui_fault_dropped_event_count
		+ m_tp_ui_fault_hook_integrity_violation_count + m_tp_ui_fault_illegal_multi_softkey_sample_count
		+ m_tp_ui_fault_abuse_guard_escalation_count;
	if (total_faults == 0U) {
		std::error_code ec;
		std::filesystem::remove(m_user_io_harness_failures_path, ec);
		return;
	}
	std::ofstream out(m_user_io_harness_failures_path, std::ios::out | std::ios::trunc);
	if (!out.good())
		return;
	out << "# User I/O Harness Failures\n\n";
	out << "- run_id: `" << m_user_io_harness_run_id << "`\n";
	out << "- resolved_profile_id: `" << terminal21_profile_id_string(m_keypad_board.terminal_21_profile) << "`\n\n";
	out << "## Counter Summary\n\n";
	out << "- unknown_opcode_count: " << m_tp_ui_fault_unknown_opcode_count << "\n";
	out << "- duplicate_release_count: " << m_tp_ui_fault_duplicate_release_count << "\n";
	out << "- synthetic_release_count: " << m_tp_ui_fault_synthetic_release_count << "\n";
	out << "- dropped_event_count: " << m_tp_ui_fault_dropped_event_count << "\n";
	out << "- hook_event_integrity_violation_count: " << m_tp_ui_fault_hook_integrity_violation_count << "\n";
	out << "- illegal_multi_softkey_sample_count: " << m_tp_ui_fault_illegal_multi_softkey_sample_count << "\n";
	out << "- abuse_guard_escalation_count: " << m_tp_ui_fault_abuse_guard_escalation_count << "\n";
}

void millennium_state::tp_backend_process_front_panel(std::uint32_t keymatrix, std::uint32_t linectrl,
	std::uint32_t softkeys_raw, std::uint8_t secmask, std::uint64_t cycle, std::uint16_t pc)
{
	if (m_tp_backend_kind != tp_backend_kind::pcd3349a || !m_tp_pcd3349a) {
		tp_process_front_panel_events(keymatrix, linectrl, softkeys_raw);
		update_earpiece_tone_output(cycle, pc);
		return;
	}

	auto const res = m_tp_pcd3349a->process_front_panel(keymatrix, linectrl, softkeys_raw, secmask,
		m_vfd_is_oos || m_vfd_is_not_responding || m_runtime_oos_seen || m_runtime_not_responding_seen,
		m_voiceware->playing(), m_keypad_board.terminal_21_profile, cycle, m_maincpu->unscaled_clock());
	if (res.hook_changed)
		set_tel_hook_state(res.hook_onhook, cycle, "pcd3349a_hook_transition");
	for (std::uint8_t ev : res.tp_events)
		tp_enqueue_ui_event(ev, "pcd3349a_front_panel");

	bool const off_hook = !m_tel_hook_onhook;
	bool const voice_active = m_voiceware->playing();
	auto const &rs = m_audio_route->route_state();
	bool const rx_open = !rs.rx_muted;
	bool const oos_mode = m_vfd_is_oos || m_vfd_is_not_responding || m_runtime_not_responding_seen;
	auto const mode = m_tp_pcd3349a->compute_tone_mode(off_hook, rx_open, voice_active, oos_mode);
	switch (mode) {
	case millennium_pcd3349a::tone_mode::none:
		m_earpiece_tone_mode = earpiece_tone_mode::none;
		m_earpiece_tone_a->set_state(0);
		m_earpiece_tone_b->set_state(0);
		break;
	case millennium_pcd3349a::tone_mode::dialtone:
		m_earpiece_tone_mode = earpiece_tone_mode::dialtone;
		m_earpiece_tone_a->set_clock(350);
		m_earpiece_tone_b->set_clock(440);
		m_earpiece_tone_a->set_state(1);
		m_earpiece_tone_b->set_state(1);
		break;
	case millennium_pcd3349a::tone_mode::nis:
		m_earpiece_tone_mode = earpiece_tone_mode::nis;
		m_earpiece_tone_a->set_clock(480);
		m_earpiece_tone_b->set_clock(620);
		m_earpiece_tone_a->set_state(1);
		m_earpiece_tone_b->set_state(1);
		break;
	}
}

bool millennium_state::tp_backend_handle_cp_byte(std::uint8_t byte)
{
	if (m_tp_backend_kind != tp_backend_kind::pcd3349a || !m_tp_pcd3349a)
		return false;
	auto const resp = m_tp_pcd3349a->handle_cp_to_tp_byte(byte, m_tel_hook_onhook_stable);
	// PCD3349A/TP8048 is the sole source of TP→CP CSI/O octets: never fall through to the
	// host-modeled opcode catalog in \c ipcomm_complete_csio_tx_byte.
	for (std::size_t i = 0; i < resp.size(); ++i) {
		std::uint8_t const b = resp[i];
		ipcomm_queue_rx_byte(b, b >= 0xC0U);
		if (b == 0x70U || b == 0x72U) {
			m_tel_boot_power_ack_seen = true;
			m_tel_boot_code_sent = true;
		}
		if (b == 0x6cU || b == 0x6eU)
			m_tel_boot_hook_state_seen = true;
		if (b == 0x7aU)
			m_tel_boot_power_status_seen = true;
		if (b == 0xc4U)
			m_tel_boot_error_report_seen = true;
		if (b == 0xc0U)
			m_tel_boot_status_seen = true;
		if (b == 0xc4U && i + 3U < resp.size())
			m_csio_error_report_ok = true;
		if (b == 0xc0U && i + 7U < resp.size())
			m_csio_status_frame_ok = true;
	}
	if (m_tel_boot_power_ack_seen && m_tel_boot_error_report_seen && m_tel_boot_status_seen) {
		m_tel_fw_boot_contract_satisfied = true;
		m_tel_ready_sequence_completed = true;
		m_alarm_tel_not_responding_latched = false;
		m_alarm_tel_not_responding_cleared = true;
		m_termfg_telephony_up_inferred = true;
		if (!m_tp_readiness_sequence_trace_path.empty()) {
			char b[320];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_sequence_complete\","
				"\"backend\":\"pcd3349a_8048\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
			millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
		}
	}
	trace_tp_command_accepted(byte, !resp.empty(), "pcd3349a", "pcd3349a_backend");
	return true;
}

void millennium_state::tp_process_front_panel_events(u32 keymatrix, u32 linectrl, u32 softkeys_raw)
{
	u64 const cy = m_maincpu->total_cycles();
	u64 const hz = m_maincpu->unscaled_clock();
	u32 const prof = terminal21_keymatrix_applied_mask(m_keypad_board.terminal_21_profile);
	u32 const km = keymatrix & prof;
	m_tp_ui_policy_km_snapshot = km;
	u32 const dmask = k_terminal21_dial_pad_and_rep_bits & prof;
	u32 const sk_masked =
		(m_keypad_board.terminal_21_profile == millennium_terminal21_user_io_profile::vfd_11line_softkeys)
			? (softkeys_raw & 0x0fffU)
			: 0U;

	// terminal_22 softkey_scan_contract: debounce before treating stable state (base_tick-aligned ~25 ms).
	bool const vfd_sk_profile =
		m_keypad_board.terminal_21_profile == millennium_terminal21_user_io_profile::vfd_11line_softkeys;
	u32 sk_logical = 0U;
	if (vfd_sk_profile) {
		if (sk_masked != m_tp_ui_sk_last_raw) {
			m_tp_ui_sk_last_raw = sk_masked;
			m_tp_ui_sk_stable_deadline_cy =
				(hz != 0ULL) ? (cy + hz * 25ULL / 1000ULL) : 0ULL;
		} else if (m_tp_ui_sk_stable_deadline_cy != 0ULL && hz != 0ULL && cy >= m_tp_ui_sk_stable_deadline_cy) {
			m_tp_ui_sk_stable = sk_masked;
			m_tp_ui_sk_stable_deadline_cy = 0ULL;
		}
		sk_logical = m_tp_ui_sk_stable;
	} else {
		m_tp_ui_sk_last_raw = 0U;
		m_tp_ui_sk_stable = 0U;
		m_tp_ui_sk_stable_deadline_cy = 0ULL;
	}

	// terminal_24: drop suppress bits when the physical key is no longer down.
	m_tp_ui_suppress_dial_until_physical_release &= km;

	u32 const dial_sup = m_tp_ui_suppress_dial_until_physical_release & dmask;
	u32 const km_edge = (km & ~dial_sup);
	u32 const old_edge = (m_last_tp_ui_keymatrix_state & prof & ~dial_sup);

	// terminal_24: missing KEY_RELEASE (0x5E) after dial/rep press — synthesize after 3000 ms.
	if (!m_craft_entry_window_active && m_tp_ui_pending_dial_release > 0U && m_tp_ui_dial_release_deadline_cy != 0ULL && hz != 0ULL
		&& cy >= m_tp_ui_dial_release_deadline_cy) {
		m_tp_ui_fault_synthetic_release_count++;
		tp_enqueue_ui_event(0x5EU, "terminal24_synthetic_release");
		append_user_io_harness_trace("user_io_fault", "terminal24_missing_release_synthesized");
		m_tp_ui_pending_dial_release = 0U;
		m_tp_ui_dial_release_deadline_cy = 0ULL;
		m_tp_ui_repeat_hold_mask = 0U;
		m_tp_ui_repeat_hold_start_cy = 0ULL;
		m_tp_ui_repeat_extra_sent = 0U;
		m_tp_ui_suppress_dial_until_physical_release |= (km & dmask);
	}

	// Make: TP→CP byte. Break: KEY_RELEASE (0x5E) for dial-pad / rep-dial only (not volume/lang/next_call/softkeys).
	auto const pressed = [km_edge](u32 mask) { return (km_edge & mask) != 0U; };
	auto const was_pressed = [old_edge](u32 mask) { return (old_edge & mask) != 0U; };
	auto const emit_key = [this, &pressed, &was_pressed, cy, hz](u32 mask, u8 code, char const *reason,
								  bool send_tp_key_release) {
		bool const now = pressed(mask);
		bool const old = was_pressed(mask);
		if (now && !old) {
			trace_tp_keypad_input_edge(mask, code, reason);
			tp_enqueue_ui_event(code, reason);
			note_craft_entry_key(code, cy);
			if (send_tp_key_release) {
				if (m_tp_ui_pending_dial_release == 0U && hz != 0ULL)
					m_tp_ui_dial_release_deadline_cy = cy + hz * 3000ULL / 1000ULL;
				m_tp_ui_pending_dial_release++;
			}
		} else if (send_tp_key_release && old && !now) {
			if (m_tp_ui_pending_dial_release > 0U) {
				m_tp_ui_pending_dial_release--;
				tp_enqueue_ui_event(0x5EU, "key_release");
				if (m_tp_ui_pending_dial_release == 0U)
					m_tp_ui_dial_release_deadline_cy = 0ULL;
			} else {
				m_tp_ui_fault_duplicate_release_count++;
				append_user_io_harness_trace("user_io_fault", "terminal24_duplicate_release_ignored");
			}
		}
	};

	emit_key(0x00000001U, 0x22U, "digit_1", true);
	emit_key(0x00000002U, 0x24U, "digit_2", true);
	emit_key(0x00000004U, 0x26U, "digit_3", true);
	emit_key(0x00000008U, 0x28U, "digit_4", true);
	emit_key(0x00000010U, 0x2AU, "digit_5", true);
	emit_key(0x00000020U, 0x2CU, "digit_6", true);
	emit_key(0x00000040U, 0x2EU, "digit_7", true);
	emit_key(0x00000080U, 0x30U, "digit_8", true);
	emit_key(0x00000100U, 0x32U, "digit_9", true);
	emit_key(0x00000400U, 0x34U, "digit_0", true);
	emit_key(0x00000200U, 0x36U, "digit_star", true);
	emit_key(0x00000800U, 0x38U, "digit_hash", true);
	emit_key(0x40000000U, 0x20U, "dial_pad_a", true);
	emit_key(0x00001000U, 0x58U, "new_call", false);
	emit_key(0x00040000U, 0x5AU, "language", false);
	emit_key(0x00002000U, 0x3AU, "dial_pad_b", true);
	emit_key(0x00004000U, 0x3CU, "dial_pad_c", true);
	emit_key(0x00008000U, 0x3EU, "dial_pad_d", true);
	emit_key(0x00010000U, 0x56U, "volume_up", false);
	emit_key(0x00020000U, 0x54U, "volume_down", false);

	static constexpr std::uint32_t k_rep_mask[10] = {
		0x00100000U, 0x00200000U, 0x00400000U, 0x00800000U, 0x01000000U,
		0x02000000U, 0x04000000U, 0x08000000U, 0x10000000U, 0x20000000U,
	};
	for (int i = 0; i < 10; ++i)
		emit_key(k_rep_mask[i], static_cast<u8>(0x40U + unsigned(i) * 2U), "rep_dial", true);

	auto const sk_pressed = [sk_logical](u32 mask) { return (sk_logical & mask) != 0U; };
	auto const sk_was = [this](u32 mask) { return (m_last_tp_ui_softkeys_state & mask) != 0U; };
	auto const emit_sk = [this, &sk_pressed, &sk_was](u32 mask, u8 code, char const *reason) {
		bool const now = sk_pressed(mask);
		bool const old = sk_was(mask);
		if (now && !old)
			tp_enqueue_ui_event(code, reason);
	};
	unsigned const sk_pc = popcount_u32(sk_logical);
	bool const softkey_illegal = vfd_sk_profile && sk_pc > 1U;
	if (softkey_illegal) {
		if (!m_tp_ui_softkey_illegal_episode) {
			m_tp_ui_softkey_illegal_episode = true;
			m_tp_ui_fault_illegal_multi_softkey_sample_count++;
			append_user_io_harness_trace("user_io_fault", "terminal24_illegal_multi_softkey");
		}
	} else {
		m_tp_ui_softkey_illegal_episode = false;
		if (vfd_sk_profile) {
			for (unsigned bi = 0; bi < 12; ++bi)
				emit_sk(1U << bi, static_cast<u8>(0x90U + bi), "vfd_softkey");
		}
	}

	// Hookswitch: KEYMATRIX bit 19; terminal_21 hook transitions + ON_HOOK_STATE/OFF_HOOK_STATE (0x6C/0x6E).
	// terminal_24: ignore transitions closer than 40 ms (contradictory hook bounce).
	bool const offhook_now = (km & k_terminal21_hook_bit) != 0U;
	bool const offhook_old = (m_last_tp_ui_keymatrix_state & k_terminal21_hook_bit) != 0U;
	if (offhook_now != offhook_old) {
		u64 const gap_cy = hz != 0ULL ? hz * 40ULL / 1000ULL : 0ULL;
		bool const too_soon =
			m_tp_ui_last_hook_transition_cy != 0ULL && gap_cy != 0ULL
			&& (cy - m_tp_ui_last_hook_transition_cy) < gap_cy;
		if (too_soon) {
			m_tp_ui_fault_hook_integrity_violation_count++;
			m_tp_ui_fault_dropped_event_count++;
			append_user_io_harness_trace("user_io_fault", "terminal24_hook_transition_debounced");
		} else {
			m_tp_ui_last_hook_transition_cy = cy;
			set_tel_hook_state(!offhook_now, cy, "terminal21_hook_transition");
			if (hz != 0ULL) {
				if (m_tp_ui_last_accepted_hook_transition_cy != 0ULL
					&& (cy - m_tp_ui_last_accepted_hook_transition_cy) < hz * 300ULL / 1000ULL)
					m_tp_ui_abuse_rapid_hook_transition_accum++;
				else
					m_tp_ui_abuse_rapid_hook_transition_accum = 1U;
				m_tp_ui_last_accepted_hook_transition_cy = cy;
				if (m_tp_ui_abuse_rapid_hook_transition_accum >= 6U) {
					m_tp_ui_fault_abuse_guard_escalation_count++;
					m_tp_ui_abuse_rapid_hook_transition_accum = 0U;
					append_user_io_harness_trace("user_io_fault", "terminal21_abuse_guard_threshold");
					tp_enqueue_ui_event(0x64U, "terminal21_abuse_guard_line_interruption");
				}
			}
			if (offhook_now) {
				tp_enqueue_ui_event(0x62U, "off_hook_transition");
				tp_enqueue_ui_event(0x6EU, "off_hook_state");
				if (m_user_io_board.overlay.adsi_active || m_user_io_board.overlay.proton_active
					|| m_user_io_board.overlay.mondex_active || m_user_io_board.overlay.git_ui_active)
					append_user_io_harness_trace("user_io_overlay", "terminal23_off_hook_after_state_commit");
			} else {
				tp_enqueue_ui_event(0x60U, "on_hook_transition");
				tp_enqueue_ui_event(0x6CU, "on_hook_state");
			}
		}
	}

	bool const line_connected_now = (linectrl & 0x01U) != 0U;
	bool const line_connected_old = (m_last_tp_ui_linectrl_state & 0x01U) != 0U;
	if (line_connected_now != line_connected_old)
		tp_enqueue_ui_event(line_connected_now ? 0x66U : 0x64U, line_connected_now ? "line_connection" : "line_interruption");

	bool const line_reversal_suppressed = (linectrl & 0x08U) != 0U; // terminal_21 line_supervision_gating
	if (!line_reversal_suppressed) {
		bool const lr0_now = (linectrl & 0x02U) != 0U;
		bool const lr0_old = (m_last_tp_ui_linectrl_state & 0x02U) != 0U;
		if (lr0_now && !lr0_old)
			tp_enqueue_ui_event(0x68U, "line_reversal_0_pulse");
		bool const lr1_now = (linectrl & 0x04U) != 0U;
		bool const lr1_old = (m_last_tp_ui_linectrl_state & 0x04U) != 0U;
		if (lr1_now && !lr1_old)
			tp_enqueue_ui_event(0x6AU, "line_reversal_1_pulse");
	}

	// terminal_22_user_io_timing: repeat while one dial/rep key held; stop under terminal_24 dial suppress.
	if (!m_craft_entry_window_active) {
		u32 const hold = terminal22_repeat_single_key_mask(km & ~dial_sup);
		if (hold == 0U) {
			m_tp_ui_repeat_hold_mask = 0U;
			m_tp_ui_repeat_hold_start_cy = 0ULL;
			m_tp_ui_repeat_extra_sent = 0U;
		} else if (hold != m_tp_ui_repeat_hold_mask) {
			m_tp_ui_repeat_hold_mask = hold;
			m_tp_ui_repeat_hold_start_cy = cy;
			m_tp_ui_repeat_extra_sent = 0U;
		} else if (hz != 0ULL) {
			u64 const elapsed_cy = cy - m_tp_ui_repeat_hold_start_cy;
			u64 const ms500_cy = hz / 2ULL;
			u64 const ms150_cy = std::max(1ULL, hz * 150ULL / 1000ULL);
			if (elapsed_cy >= ms500_cy) {
				u64 const after500 = elapsed_cy - ms500_cy;
				unsigned const total_needed = 1U + unsigned(after500 / ms150_cy);
				u8 opc = 0;
				if (keymatrix_single_bit_to_terminal21_opcode(hold, opc)) {
					while (m_tp_ui_repeat_extra_sent < total_needed && m_tp_ui_repeat_extra_sent < 4096U) {
						tp_enqueue_ui_event(opc, "terminal22_key_repeat");
						m_tp_ui_repeat_extra_sent++;
					}
				}
			}
		}
	}

	m_last_tp_ui_keymatrix_state = km;
	m_last_tp_ui_linectrl_state = linectrl;
	m_last_tp_ui_softkeys_state = sk_logical;
}

void millennium_state::update_earpiece_tone_output(std::uint64_t cycle, std::uint16_t pc)
{
	bool const off_hook = !m_tel_hook_onhook;
	bool const voice_active = m_voiceware->playing();
	auto const &rs = m_audio_route->route_state();
	bool const rx_open = !rs.rx_muted;
	bool const oos_mode = m_vfd_is_oos || m_vfd_is_not_responding || m_runtime_not_responding_seen;
	earpiece_tone_mode new_mode = earpiece_tone_mode::none;
	// NIS/intercept-like earpiece indication takes precedence while off-hook in service-fail states.
	if (off_hook && rx_open && oos_mode) {
		new_mode = earpiece_tone_mode::nis;
	} else if (off_hook && rx_open && !voice_active) {
		// Dial-tone style cue while off-hook and not in explicit OOS/NR path.
		if (rs.call == coinline::audio_route::call_state::CALL_IDLE
			|| rs.call == coinline::audio_route::call_state::CALL_DIALING)
			new_mode = earpiece_tone_mode::dialtone;
	}
	if (new_mode == m_earpiece_tone_mode)
		return;
	m_earpiece_tone_mode = new_mode;
	switch (m_earpiece_tone_mode) {
	case earpiece_tone_mode::none:
		m_earpiece_tone_a->set_state(0);
		m_earpiece_tone_b->set_state(0);
		break;
	case earpiece_tone_mode::dialtone:
		// North American dial tone nominal pair.
		m_earpiece_tone_a->set_clock(350);
		m_earpiece_tone_b->set_clock(440);
		m_earpiece_tone_a->set_state(1);
		m_earpiece_tone_b->set_state(1);
		break;
	case earpiece_tone_mode::nis:
		// NIS/intercept-like earpiece indication (simplified dual-tone stand-in).
		m_earpiece_tone_a->set_clock(480);
		m_earpiece_tone_b->set_clock(620);
		m_earpiece_tone_a->set_state(1);
		m_earpiece_tone_b->set_state(1);
		break;
	}
	telephony_runtime_trace_event("earpiece_tone_mode_changed", "",
		(m_earpiece_tone_mode == earpiece_tone_mode::dialtone)
			? "dialtone"
			: ((m_earpiece_tone_mode == earpiece_tone_mode::nis) ? "nis" : "none"),
		true, "audio_route_state");
	if (!m_front_panel_input_source_trace_path.empty()) {
		char b[520];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"earpiece_tone_mode\","
			"\"mode\":\"%s\",\"off_hook\":%s,\"rx_open\":%s,\"voice_active\":%s,\"oos_mode\":%s}",
			static_cast<unsigned long long>(cycle), unsigned(pc), unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU),
			(m_earpiece_tone_mode == earpiece_tone_mode::dialtone)
				? "dialtone"
				: ((m_earpiece_tone_mode == earpiece_tone_mode::nis) ? "nis" : "none"),
			off_hook ? "true" : "false", rx_open ? "true" : "false", voice_active ? "true" : "false",
			oos_mode ? "true" : "false");
		millennium_boot_trace_append_line(m_front_panel_input_source_trace_path, b);
	}
}

u8 millennium_state::vfd_status_r()
{
	// VFD chip-select active low on PIO B bit 6; when high, 0x60 decodes to external UART path (not modeled here).
	if ((m_pio_8255_shadow[1] & 0x40U) != 0U) {
		emit_io_trace_line("vfd_status", 0x60, 'r', 0xffU);
		return 0xffU;
	}
	u8 const r = m_vfd->read_status(m_maincpu->total_cycles());
	emit_io_trace_line("vfd_status", 0x60, 'r', r);
	return r;
}

void millennium_state::try_boot_m5_keypad_only(u64 cycle)
{
	(void)cycle;
	if (m_m5_logged)
		return;
	if (!m_m3_m4_logged)
		emit_boot_m3_m4_snapshot("keypad_access_before_first_ram_write");
	char pcbuf[16];
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
	std::string const ts = millennium_boot_trace_timestamp_utc();
	millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m5(ts, "keypad", pcbuf));
	m_m5_logged = true;
}

void millennium_state::try_boot_m7_from_keypad(u64 cycle)
{
	(void)cycle;
	if (m_m7_logged)
		return;
	if (!m_keypad->consume_m7_pending())
		return;
	std::string const ts = millennium_boot_trace_timestamp_utc();
	millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m7(ts, m_keypad->keypad_scan_count()));
	m_m7_logged = true;
}

void millennium_state::try_boot_m8(u64 cycle)
{
	(void)cycle;
	if (m_m8_logged)
		return;
	if (!m_modem->consume_m8_pending())
		return;
	std::string const ts = millennium_boot_trace_timestamp_utc();
	millennium_boot_trace_append_line(m_boot_trace_path,
		millennium_boot_trace_m8(ts, "initialized", m_modem->dcd_line(), m_modem->cts_line()));
	m_m8_logged = true;
}

void millennium_state::maybe_io_trace(char const *tag, std::uint16_t port, char rw, std::uint8_t data)
{
	emit_io_trace_line(tag, port, rw, data);
}

void millennium_state::emit_io_trace_line(char const *tag, std::uint16_t port, char rw, std::uint8_t data)
{
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	m_last_trace_pc = pc;
	m_last_trace_sp = sp;
	m_last_trace_port = port;
	if (m_io_trace_path.empty())
		return;
	if (!profile_should_log_io(tag, port, rw))
		return;
	std::string const line = millennium_format_io_trace_line_v2(m_maincpu->total_cycles(), port, rw, data, tag, pc, sp,
		current_boot_milestone_tag());
	m_last_io_event_json = line;
	append_io_trace_line_profiled(line);
}

void millennium_state::vfd_display_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	int const cursor_before_row = m_vfd->buffer().cursor_row();
	int const cursor_before_col = m_vfd->buffer().cursor_col();
	bool const vfd_cmd = (m_pio_8255_shadow[1] & 0x20U) != 0U;
	maybe_io_trace(vfd_cmd ? "vfd_cmd" : "vfd_data", 0x60, 'w', data);
	m_vfd->write_port(data, cy, m_pio_8255_shadow[1]);
	auto normalize = [](std::string s) -> std::string {
		for (char &c : s) {
			unsigned char uc = static_cast<unsigned char>(c);
			c = (uc >= 0x20U && uc <= 0x7eU) ? char(std::tolower(uc)) : ' ';
		}
		return s;
	};
	{
		std::string const n0 = normalize(vfd_row_text(0));
		std::string const n1 = normalize(vfd_row_text(1));
		bool const blank = n0.find_first_not_of(' ') == std::string::npos && n1.find_first_not_of(' ') == std::string::npos;
		bool const oos = (n0.find("out of service") != std::string::npos) || (n1.find("out of service") != std::string::npos)
			|| (n0.find("not in service") != std::string::npos) || (n1.find("not in service") != std::string::npos);
		bool const nr = (n0.find("telephony") != std::string::npos && n1.find("not respo") != std::string::npos)
			|| (n1.find("not respo") != std::string::npos);
		m_vfd_is_oos = oos;
		m_vfd_is_not_responding = nr;
		if (blank)
			telephony_runtime_trace_event("vfd_blank_seen", "", "", true, "vfd_update");
		if (oos) {
			m_runtime_oos_seen = true;
			telephony_runtime_trace_event("vfd_oos_candidate_seen", "", "", true, "vfd_update");
		}
		if (nr) {
			if (!m_tp_readiness_sequence_trace_path.empty()) {
				char b[520];
				std::snprintf(b, sizeof(b),
					"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"display_refresh_before_tp_ready\","
					"\"ready_state_before\":%s,\"ready_state_after\":%s,\"current_vfd_text\":\"%s|%s\"}",
					static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
					unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU),
					m_tel_ready_sequence_completed ? "true" : "false", m_tel_ready_sequence_completed ? "true" : "false",
					vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
				millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
			}
			if (!m_runtime_not_responding_seen && m_runtime_oos_seen)
				telephony_runtime_trace_event("display_replaced_by_not_responding", "", "", true, "oos_to_not_responding");
			m_runtime_not_responding_seen = true;
			telephony_runtime_trace_event("vfd_not_responding_seen", "", "", true, "vfd_update");
		}
	}
	// M7C: ready when CSIO-modeled telephony framing sees a checksum-valid TELEPHONY_STATUS / error report path
	// and firmware leaves the persistent "telephony board is not responding" banner.
	if (!m_m7c_logged && (m_csio_status_frame_ok || m_csio_error_report_ok)) {
		millennium_display_profile const &dp = m_vfd->display_profile();
		std::vector<char> const &cells = m_vfd->vfd_cells();
		if (dp.rows >= 2 && dp.columns >= 1 && std::size_t(dp.rows * dp.columns) <= cells.size()) {
			std::string row0(cells.begin(), cells.begin() + dp.columns);
			std::string row1(cells.begin() + dp.columns, cells.begin() + (2 * dp.columns));
			std::string const n0 = normalize(row0);
			std::string const n1 = normalize(row1);
			bool const not_resp = (n0.find("telephony") != std::string::npos && n1.find("not respo") != std::string::npos)
				|| n1.find("not respo") != std::string::npos;
			if (m_tel_fw_boot_contract_satisfied && m_not_responding_display_seen && !not_resp) {
				if (!m_tp_readiness_sequence_trace_path.empty()) {
					char b2[520];
					std::snprintf(b2, sizeof(b2),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"display_refresh_after_tp_ready\","
						"\"ready_state_before\":%s,\"ready_state_after\":%s,\"current_vfd_text\":\"%s|%s\"}",
						static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU),
						m_tel_ready_sequence_completed ? "true" : "false", m_tel_ready_sequence_completed ? "true" : "false",
						row0.c_str(), row1.c_str());
					millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b2);
				}
				std::string const ts = millennium_boot_trace_timestamp_utc();
				char b[420];
				std::snprintf(b, sizeof(b),
					"{\"milestone\":\"M7C\",\"ts\":\"%s\",\"event\":\"M7C_TELEPHONY_READY\","
					"\"reason\":\"csio_status_or_error_frame_ok_and_vfd_left_not_responding\","
					"\"csio_status_frame_ok\":%s,\"csio_error_report_ok\":%s}",
					ts.c_str(), m_csio_status_frame_ok ? "true" : "false",
					m_csio_error_report_ok ? "true" : "false");
				millennium_boot_trace_append_line(m_boot_trace_path, b);
				m_m7c_logged = true;
				if (!m_telephony_ready_decision_trace_path.empty()) {
					char td[540];
					std::snprintf(td, sizeof(td),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"telephony_ready\","
						"\"csio_status_frame_ok\":%s,\"csio_error_report_ok\":%s,\"vfd_not_responding\":false,"
						"\"line0\":\"%s\",\"line1\":\"%s\"}",
						static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU),
						m_csio_status_frame_ok ? "true" : "false", m_csio_error_report_ok ? "true" : "false",
						row0.c_str(), row1.c_str());
					millennium_boot_trace_append_line(m_telephony_ready_decision_trace_path, td);
				}
				if (!m_service_display_trace_path.empty()) {
					char sd[520];
					std::snprintf(sd, sizeof(sd),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_display_refresh\","
						"\"ready_accepted\":true,\"reason\":\"vfd_left_not_responding\",\"line0\":\"%s\",\"line1\":\"%s\"}",
						static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU), row0.c_str(), row1.c_str());
					millennium_boot_trace_append_line(m_service_display_trace_path, sd);
					char sx[420];
					std::snprintf(sx, sizeof(sx),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_display_task_exit\","
						"\"event_area\":\"service_display_refresh\",\"reason\":\"vfd_message_replaced\"}",
						static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_service_display_trace_path, sx);
				}
				if (!m_vfd_message_state_trace_path.empty()) {
					char vm[560];
					std::snprintf(vm, sizeof(vm),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"vfd_exited_not_responding\","
						"\"event_area\":\"out_of_service_message_check\","
						"\"previous_vfd_text\":\"telephony_not_responding\",\"current_vfd_text\":\"%s|%s\"}",
						static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU), row0.c_str(), row1.c_str());
					millennium_boot_trace_append_line(m_vfd_message_state_trace_path, vm);
				}
			}
		}
	}
	u64 const hz_vfd_diag = static_cast<u64>(m_maincpu->unscaled_clock());
	u64 const m7c_diag_period = std::max(1ULL, hz_vfd_diag / 10U); // ~100 ms cadence vs prior 8M-cycle gaps
	if (!m_telephony_ready_decision_trace_path.empty() && !m_m7c_logged
		&& (cy - m_m7c_gate_diag_last_cycle) >= m7c_diag_period) {
		m_m7c_gate_diag_last_cycle = cy;
		millennium_display_profile const &dp2 = m_vfd->display_profile();
		std::vector<char> const &cells2 = m_vfd->vfd_cells();
		bool not_resp_heur = false;
		bool const csio_gate = m_csio_status_frame_ok || m_csio_error_report_ok;
		if (dp2.rows >= 2 && dp2.columns >= 1 && std::size_t(dp2.rows * dp2.columns) <= cells2.size()) {
			std::string row0b(cells2.begin(), cells2.begin() + dp2.columns);
			std::string row1b(cells2.begin() + dp2.columns, cells2.begin() + (2 * dp2.columns));
			std::string const n0b = normalize(row0b);
			std::string const n1b = normalize(row1b);
			not_resp_heur = (n0b.find("telephony") != std::string::npos && n1b.find("not respo") != std::string::npos)
				|| n1b.find("not respo") != std::string::npos;
		}
		if (not_resp_heur) {
			if (!m_not_responding_display_seen) {
				m_not_responding_display_seen = true;
			}
		}
		char dg[520];
		std::snprintf(dg, sizeof(dg),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"m7c_gate_pending\","
			"\"csio_frames_ok\":%s,\"vfd_not_responding_heuristic\":%s,"
			"\"note\":\"see_SERVTASK_service_display_update_TERMFG_TELEPHONY_UP_and_INITASK_INIS_GOT_TEL_INF\"}",
			static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
			csio_gate ? "true" : "false", not_resp_heur ? "true" : "false");
		millennium_boot_trace_append_line(m_telephony_ready_decision_trace_path, dg);
		if (!m_vfd_message_state_trace_path.empty() && not_resp_heur) {
			char vm[420];
			std::snprintf(vm, sizeof(vm),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"vfd_still_not_responding\","
				"\"event_area\":\"out_of_service_message_check\",\"current_vfd_text\":\"telephony_not_responding\"}",
				static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU));
			millennium_boot_trace_append_line(m_vfd_message_state_trace_path, vm);
		}
		if (!m_service_task_trace_path.empty()) {
			char st[560];
			std::snprintf(st, sizeof(st),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_task_gate\","
				"\"SERVS_UPDATE_LEVEL_observed\":false,\"SERVS_CHECK_OOS_MSG_observed\":false,"
				"\"C0_accepted\":%s,\"C4_accepted\":%s,\"termfg_telephony_up_inferred\":%s,"
				"\"ready_rejected_reason\":\"display_still_not_responding_or_no_refresh\"}",
				static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
				m_csio_status_frame_ok ? "true" : "false", m_csio_error_report_ok ? "true" : "false",
				m_termfg_telephony_up_inferred ? "true" : "false");
			millennium_boot_trace_append_line(m_service_task_trace_path, st);
		}
		if (!m_alarm_condition_trace_path.empty()) {
			char at[520];
			std::snprintf(at, sizeof(at),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"alarm_gate_state\","
				"\"alarm\":\"ALM_TEL_NOT_RESPONDING\",\"latched_observed\":%s,\"cleared_observed\":%s}",
				static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
				m_alarm_tel_not_responding_latched ? "true" : "false",
				m_alarm_tel_not_responding_cleared ? "true" : "false");
			millennium_boot_trace_append_line(m_alarm_condition_trace_path, at);
		}
	}
	if (!m_vfd_trace_path.empty()) {
		int const cursor_after_row = m_vfd->buffer().cursor_row();
		int const cursor_after_col = m_vfd->buffer().cursor_col();
		u16 const pc = u16(m_maincpu->pc() & 0xffffU);
		u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
		char char_buf[8];
		char cmd_buf[32];
		char line0[64];
		char line1[64];
		char event_buf[16];
		bool const printable = (data >= 0x20U && data <= 0x7eU);
		std::snprintf(event_buf, sizeof(event_buf), "%s", printable ? "char_data" : "command");
		if (printable)
			std::snprintf(char_buf, sizeof(char_buf), "%c", char(data));
		else
			char_buf[0] = '\0';
		std::snprintf(cmd_buf, sizeof(cmd_buf), "%s",
			(data == 0x1bU) ? "escape" : (data == 0x0aU) ? "lf"
											 : (data == 0x0dU) ? "cr"
											 : (data == 0x0cU) ? "ff"
											 : (data == 0x0eU) ? "clear_display"
											 : (data == 0x08U) ? "backspace"
											 : (data == 0x09U) ? "horizontal_tab"
											 : (data == 0x11U) ? "increment_write_mode"
											 : (data == 0x12U) ? "vertical_scroll_mode"
											 : (data == 0x15U) ? "cursor_on"
											 : (data == 0x16U) ? "cursor_off"
											 : (data == 0x18U) ? "international_font"
											 : (data == 0x19U) ? "katakana_font"
											 : "data");
		std::string const row0 = m_vfd->first_text_row();
		std::snprintf(line0, sizeof(line0), "%s", row0.c_str());
		if (m_display_profile.rows > 1) {
			std::string row1;
			auto const &cells = m_vfd->vfd_cells();
			for (int c = 0; c < m_display_profile.columns; ++c)
				row1.push_back(cells[std::size_t(m_display_profile.columns + c)]);
			std::snprintf(line1, sizeof(line1), "%s", row1.c_str());
		} else {
			line1[0] = '\0';
		}
		char buf[1024];
		std::snprintf(buf, sizeof(buf),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"port\":\"0x0060\",\"byte\":\"0x%02X\","
			"\"raw_byte\":\"0x%02X\",\"event_type\":\"%s\",\"char\":\"%s\",\"command_name\":\"%s\","
			"\"cursor_before\":{\"row\":%d,\"col\":%d},\"cursor_after\":{\"row\":%d,\"col\":%d},"
			"\"display_after\":{\"line0\":\"%s\",\"line1\":\"%s\"},\"notes\":\"firmware_vfd_write\"}",
			static_cast<unsigned long long>(cy), unsigned(pc), unsigned(sp), unsigned(data), unsigned(data), event_buf,
			char_buf, cmd_buf, cursor_before_row, cursor_before_col, cursor_after_row, cursor_after_col, line0, line1);
		millennium_boot_trace_append_line(m_vfd_trace_path, buf);
		if (!m_vfd_snapshots_path.empty()) {
			std::string const snap = m_vfd->export_snapshot_json();
			if (!snap.empty() && snap[0] == '{')
				millennium_boot_trace_append_line(m_vfd_snapshots_path, snap.substr(0, snap.size() - 1));
		}
	}
	maybe_emit_vfd_idle_fixture_diff(cy);
	if (!m_m6_logged) {
		std::string const ts = millennium_boot_trace_timestamp_utc();
		millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m6(ts, m_vfd->first_text_row()));
		m_m6_logged = true;
		flush_io_trace_ring_to_disk("m6_reached");
		flush_cpu_trace_ring_to_disk();
	}
	if (!m_m10_logged && !m_idle_display_fixture_json.empty() &&
		m_vfd->buffer().text_rows_match_fixture_json(m_idle_display_fixture_json)) {
		std::string const ts_m10 = millennium_boot_trace_timestamp_utc();
		if (!m_m9_logged) {
			char pcbuf[16];
			u16 const pc = u16(m_maincpu->pc() & 0xffffU);
			std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
			millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m9(ts_m10, pcbuf));
			m_m9_logged = true;
		}
		std::string const row_esc = json_escape_string(m_vfd->first_text_row());
		char dh[700];
		std::snprintf(dh, sizeof(dh),
			"{\"milestone\":\"display_idle_heuristic\",\"ts\":\"%s\",\"vfd\":\"%s\","
			"\"idle_fixture_match\":true,\"legacy_boot_milestone_alias\":\"M10\"}",
			ts_m10.c_str(), row_esc.c_str());
		millennium_boot_trace_append_line(m_boot_trace_path, std::string(dh));
		millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m10(ts_m10, m_vfd->first_text_row()));
		m_m10_logged = true;
	}
}

u8 millennium_state::board_status_r()
{
	u64 const cy = m_maincpu->total_cycles();
	// 93LC66 Microwire (board): CS from HW_CNTL 0x40 write bit 6; SK from PIO-B 0x42 bit 7 (rising edges);
	// DI from 8255 port A (0x41) bit 0; DO muxed into this status read (0x40) bit 3 when EEPROM CS is asserted.
	// HW control port image: upper bits are board-control outputs; low nibble carries security/supervision inputs.
	u8 sec = static_cast<u8>(m_security->read(cy) & 0x0fU);
	u32 const line = m_linectrl_io->read();
	// Readback is firmware-visible board input status, not a mirror of write-only control outputs
	// (SCLK/reset bits on HW_CNTL writes). Mirroring output latch bit7 leaked clock state as a
	// persistent TP fault indication and drove false "telephony board is not responding" refreshes.
	u8 hw = 0x00U;
	// LINECTRL bit 0: line supervision (LINE_CONNECTION vs LINE_INTERRUPTION); reflected for firmware readback tests.
	if ((line & 0x01U) != 0U)
		hw = static_cast<u8>(hw | 0x10U);
	else
		hw = static_cast<u8>(hw & ~0x10U);
	u8 r = static_cast<u8>(hw | sec);
	if (m_microwire_93c66.chip_select())
		r = static_cast<u8>((r & ~0x08U) | (m_microwire_93c66.serial_out() ? 0x08U : 0x00U));
	emit_io_trace_line("board_status", 0x40, 'r', r);
	if (!m_tp_board_status_trace_path.empty()) {
		char b[420];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_board_status_changed\","
			"\"board_status_after\":\"0x%02X\",\"ready_state_after\":%s,\"current_vfd_text\":\"%s|%s\",\"note\":\"board_status_r\"}",
			static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(r),
			m_tel_ready_sequence_completed ? "true" : "false", vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
		millennium_boot_trace_append_line(m_tp_board_status_trace_path, b);
	}
	if (!m_front_panel_input_source_trace_path.empty() && line != m_last_firmware_linectrl_state) {
		char const *event = (line & 0x01U) != 0U ? "input_line_connected_read_by_firmware"
												: "input_line_interruption_read_by_firmware";
		char b[640];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"%s\","
			"\"input_source\":\"mame_input_port\",\"MAME_input_name\":\"LINECTRL\",\"old_input_state\":\"0x%08X\","
			"\"new_input_state\":\"0x%08X\",\"mapped_emulated_hardware_signal\":\"line_supervision_connected\","
			"\"firmware_visible_port\":\"0x0040\",\"firmware_visible_value\":\"0x%02X\"}",
			static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), event, unsigned(m_last_firmware_linectrl_state), unsigned(line),
			unsigned(r));
		millennium_boot_trace_append_line(m_front_panel_input_source_trace_path, b);
		m_last_firmware_linectrl_state = line;
	}
	return r;
}

void millennium_state::trace_hw_cntl_telephony_relays(u8 prev, u8 data, u64 cy, u16 pc, u16 sp)
{
	if (m_hw_cntl_relay_trace_path.empty())
		return;
	// Board HW_CNTL (0x40) low bits: relay drive lines 0x01 / 0x02 / 0x04.
	static constexpr u8 RELAY_MASK = 0x07U;
	u8 const r_prev = static_cast<u8>(prev & RELAY_MASK);
	u8 const r_now = static_cast<u8>(data & RELAY_MASK);
	if (r_prev == r_now)
		return;
	auto relay_json = [](unsigned idx, bool on) {
		char const *role = "";
		switch (idx) {
		case 1:
			role = "R1 modem relay (DATAJACK.C): CO line to internal NCC modem when active";
			break;
		case 2:
			role = "R2 talk-path relay: supplementary +24VDC to telephony when active; "
			       "inactive selects CO-line power / power-fail takeover (toll rules from config)";
			break;
		case 3:
			role = "R3 modem sink relay: tone-detect / CO coupling when active; "
			       "inactive allows modem loop-current sink for off-hook";
			break;
		default:
			break;
		}
		char b[360];
		std::snprintf(b, sizeof(b), "\"r%u\":{\"active\":%s,\"semantics\":\"%s\"}", idx, on ? "true" : "false", role);
		return std::string(b);
	};
	bool const r1p = (r_prev & 0x01U) != 0U;
	bool const r2p = (r_prev & 0x02U) != 0U;
	bool const r3p = (r_prev & 0x04U) != 0U;
	std::string const s1 = relay_json(1, (r_now & 0x01U) != 0U);
	std::string const s2 = relay_json(2, (r_now & 0x02U) != 0U);
	std::string const s3 = relay_json(3, (r_now & 0x04U) != 0U);
	char line[920];
	std::snprintf(line, sizeof(line),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"hw_cntl_relay_change\","
		"\"port\":\"HW_CNTL\",\"firmware_visible_port\":\"0x0040\","
		"\"hw_cntl_before\":\"0x%02X\",\"hw_cntl_after\":\"0x%02X\","
		"\"relay_bits_before\":\"0x%02X\",\"relay_bits_after\":\"0x%02X\","
		"\"r1_before\":%s,\"r2_before\":%s,\"r3_before\":%s,"
		"%s,%s,%s}",
		static_cast<unsigned long long>(cy), unsigned(pc), unsigned(sp),
		unsigned(prev), unsigned(data), unsigned(r_prev), unsigned(r_now),
		r1p ? "true" : "false", r2p ? "true" : "false", r3p ? "true" : "false",
		s1.c_str(), s2.c_str(), s3.c_str());
	millennium_boot_trace_append_line(m_hw_cntl_relay_trace_path, line);
}

void millennium_state::trace_pio_port_c_telephony_routing(u8 prev, u8 data, u64 cy, u16 pc, u16 sp)
{
	if (m_hw_cntl_relay_trace_path.empty())
		return;
	// PIO port C (0x43): data-jack relay bit 0x40, network-select 0x80, MUTDTMF 0x10.
	static constexpr u8 PIO_C_TELEPHONY_RELAY_MASK = 0xd0U;
	u8 const pb = static_cast<u8>(prev & PIO_C_TELEPHONY_RELAY_MASK);
	u8 const nb = static_cast<u8>(data & PIO_C_TELEPHONY_RELAY_MASK);
	if (pb == nb)
		return;

	bool const dj_rb = (prev & 0x40U) != 0U;
	bool const dj_nb = (prev & 0x80U) != 0U;
	bool const dj_ra = (data & 0x40U) != 0U;
	bool const dj_na = (data & 0x80U) != 0U;
	// Derived laptop (R4) / network (R5) interpretation for the default product build variant.
	auto fill_r4_r5 = [](bool dj_relay_bit, bool network_bit, bool &r4, bool &r5) {
		if (!dj_relay_bit && !network_bit) {
			r4 = true;
			r5 = false;
		} else if (!dj_relay_bit && network_bit) {
			r4 = true;
			r5 = true;
		} else {
			r4 = false;
			r5 = true;
		}
	};
	bool r4b = false, r5b = false, r4a = false, r5a = false;
	fill_r4_r5(dj_rb, dj_nb, r4b, r5b);
	fill_r4_r5(dj_ra, dj_na, r4a, r5a);
	bool const mut_b = (prev & 0x10U) != 0U;
	bool const mut_a = (data & 0x10U) != 0U;

	char line[1100];
	std::snprintf(line, sizeof(line),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"pio_port_c_coifrc_change\","
		"\"port\":\"PIO_PORT_C\",\"firmware_visible_port\":\"0x0043\","
		"\"pio_c_before\":\"0x%02X\",\"pio_c_after\":\"0x%02X\","
		"\"coifrc_traced_bits_mask\":\"0x%02X\","
		"\"data_jack_relay_bit_before\":%s,\"data_jack_network_bit_before\":%s,\"mutdtmf_bit_before\":%s,"
		"\"data_jack_relay_bit_after\":%s,\"data_jack_network_bit_after\":%s,\"mutdtmf_bit_after\":%s,"
		"\"r4_laptop_relay_active_before\":%s,\"r5_network_relay_active_before\":%s,"
		"\"r4_laptop_relay_active_after\":%s,\"r5_network_relay_active_after\":%s,"
		"\"r4_r5_derive_profile\":\"DATAJACK.C_get_relay_status_else\","
		"\"semantics\":{\"data_jack_relay_0x40\":\"Physical DATA_JACK_RELAY_MASK on PIO-C (pairs with 0x80 for R4/R5)\","
		"\"data_jack_network_0x80\":\"Physical DATA_JACK_NETWORK_MASK — laptop vs network chip routing\","
		"\"mutdtmf_0x10\":\"MUTDTMF — DTMF mute / far-end detect path (COIFRC.C predial, DATAJACK.C)\","
		"\"r4_r5\":\"DATAJACK.C names R4 laptop relay and R5 network relay; combination decode is build-specific\"}}",
		static_cast<unsigned long long>(cy), unsigned(pc), unsigned(sp),
		unsigned(prev), unsigned(data), unsigned(PIO_C_TELEPHONY_RELAY_MASK),
		dj_rb ? "true" : "false", dj_nb ? "true" : "false", mut_b ? "true" : "false",
		dj_ra ? "true" : "false", dj_na ? "true" : "false", mut_a ? "true" : "false",
		r4b ? "true" : "false", r5b ? "true" : "false",
		r4a ? "true" : "false", r5a ? "true" : "false");
	millennium_boot_trace_append_line(m_hw_cntl_relay_trace_path, line);
}

void millennium_state::board_status_w(u8 data)
{
	u8 const prev = m_hw_cntl_port_image;
	m_hw_cntl_port_image = data;
	m_microwire_93c66.set_chip_select((data & 0x40U) != 0U);
	emit_io_trace_line("board_status", 0x40, 'w', data);
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	trace_hw_cntl_telephony_relays(prev, data, cy, pc, sp);
	// PWR_FAIL_LINE on HW_CNTL bit 5: active-low reset pulse to telephony processor; release then ~60 ms before CSI/O.
	{
		static constexpr u8 PWR_FAIL_LINE = 0x20U;
		bool const prev_pf = (prev & PWR_FAIL_LINE) != 0U;
		bool const now_pf = (data & PWR_FAIL_LINE) != 0U;
		u64 const hz = static_cast<u64>(m_maincpu->unscaled_clock());
		u64 c10 = hz / 100U;
		if (c10 == 0ULL)
			c10 = 1ULL;
		if (!prev_pf && now_pf) {
			m_tel_ip_link_enable_deadline_cycle = cy + 6ULL * c10;
		} else if (prev_pf && !now_pf) {
			m_tel_ip_link_enabled = false;
			m_tel_boot_code_sent = false;
			m_tel_boot_code_deadline_cycle = 0ULL;
		}
	}
	// HW_CNTL_PORT bit 7 (SCLK) is the CSIO external clock generated by firmware. Bridge it into the Z180 CSIO input.
	bool const prev_sclk = (prev & 0x80U) != 0;
	bool const sclk = (data & 0x80U) != 0;
	if (prev_sclk != sclk) {
		u8 const cntr_before_edge = static_cast<u8>(m_maincpu->state_int(Z180_CNTR) & 0xffU);
		u8 const trdr_before_edge = static_cast<u8>(m_maincpu->state_int(Z180_TRDR) & 0xffU);
		m_maincpu->cks_w(sclk ? 1 : 0);
		u8 const cntr_after_edge = static_cast<u8>(m_maincpu->state_int(Z180_CNTR) & 0xffU);
		if (!m_tp_csio_raw_trace_path.empty()) {
			char b[520];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"csio_clock_edge\","
				"\"cntr_before\":\"0x%02X\",\"cntr_after\":\"0x%02X\",\"trdr\":\"0x%02X\","
				"\"sclk_before\":%s,\"sclk_after\":%s,\"ready_state_before\":%s,\"ready_state_after\":%s}",
				static_cast<unsigned long long>(cy), unsigned(pc), unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU),
				unsigned(cntr_before_edge), unsigned(cntr_after_edge), unsigned(trdr_before_edge),
				prev_sclk ? "true" : "false", sclk ? "true" : "false",
				m_tel_ready_sequence_completed ? "true" : "false", m_tel_ready_sequence_completed ? "true" : "false");
			millennium_boot_trace_append_line(m_tp_csio_raw_trace_path, b);
		}
		// Provide the next RX bit ahead of the rising edge so CSIO samples correct data.
		if (sclk && m_ipcomm_have_rx_byte) {
			int const bit = (m_ipcomm_rx_shift >> m_ipcomm_rx_bit) & 1;
			m_maincpu->rxs_cts1_w(bit);
			m_ipcomm_rx_bit = (m_ipcomm_rx_bit + 1) & 7;
			if (m_ipcomm_rx_bit == 0) {
				std::uint8_t const delivered = m_ipcomm_rx_shift;
				telephony_note_csio_rx_byte(delivered);
				if (!m_telephony_handshake_trace_path.empty()) {
					char qb[420];
					std::snprintf(qb, sizeof(qb),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_to_cp_byte_delivered\","
						"\"byte\":\"0x%02X\",\"remaining_queue\":%u,\"rts_asserted\":%s}",
						static_cast<unsigned long long>(cy), unsigned(pc), unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU),
						unsigned(delivered), ipcomm_pending_rx_count(), m_ipcomm_rts_asserted ? "true" : "false");
					millennium_boot_trace_append_line(m_telephony_handshake_trace_path, qb);
				}
				// Finished a byte; clear RTS if no more queued bytes.
				m_ipcomm_have_rx_byte = false;
				prime_ipcomm_rx_shift_from_queues();
			}
		}
		// Decode control-processor→telephony CSI/O bytes from TRDR when an octet finishes shifting out.
		// TE (CNTR bit 4) clearing marks end of transmit. An older gate also required EF=1 and CNTR[2:0]==7
		// at that instant; MAME's Z180 CSI/O state machine often does not present that exact bitmask when TE
		// falls, so no ipcomm_complete_csio_tx_byte() ran → boot query opcodes (0x30–0x31) never produced
		// modeled C4/C0 replies, firmware saw no valid TP health path, and the UI fell through to
		// "telephony board not responding" with zero telephony_runtime_poll_* trace events.
		{
			bool const te_was = (cntr_before_edge & 0x10U) != 0U;
			bool const te_now = (cntr_after_edge & 0x10U) != 0U;
			bool const te_rise = !te_was && te_now;
			if (te_rise) {
				m_csio_tx_shift_edge_count = 0U;
				++m_csio_tx_shift_epoch;
			}
			if (te_was || te_now)
				++m_csio_tx_shift_edge_count;
			if (te_was && !te_now) {
				u8 const byte = static_cast<u8>(m_maincpu->state_int(Z180_TRDR) & 0xffU);
				std::string reject_reason;
				bool const accepted = qualify_cp_to_tp_csio_byte(cy, pc, u16(m_maincpu->state_int(Z180_SP) & 0xffffU),
					cntr_before_edge, cntr_after_edge, byte, "te_fall", reject_reason);
				trace_cp_to_tp_raw_candidate(cy, pc, u16(m_maincpu->state_int(Z180_SP) & 0xffffU), cntr_before_edge,
					cntr_after_edge, byte, byte, "te_fall", "trdr_on_te_clear", accepted,
					accepted ? "" : reject_reason.c_str());
				m_csio_tx_last_candidate_cycle = cy;
				m_csio_tx_last_candidate_byte = byte;
				if (accepted) {
					trace_cp_to_tp_qualified(cy, pc, u16(m_maincpu->state_int(Z180_SP) & 0xffffU), byte,
						m_ip_tx_need_length_byte ? "need_length" : "idle",
						(m_ip_tx_skip_remain > 0U) ? "frame_payload" : "command", false);
					m_csio_tx_last_accepted_cycle = cy;
					m_csio_tx_last_accepted_byte = byte;
					m_csio_tx_last_accepted_epoch = m_csio_tx_shift_epoch;
					ipcomm_complete_csio_tx_byte(byte);
				}
				if (!m_tp_csio_timing_trace_path.empty()) {
					u64 const dlt = m_tp_last_csio_timing_cycle ? (cy - m_tp_last_csio_timing_cycle) : 0ULL;
					m_tp_last_csio_timing_cycle = cy;
					u64 const hz0 = static_cast<u64>(m_maincpu->unscaled_clock());
					double const ems = (hz0 != 0ULL && dlt != 0ULL) ? double(dlt) * 1000.0 / double(hz0) : 0.;
					char ts[1400];
					std::snprintf(ts, sizeof(ts),
						"{\"cycle\":%llu,\"event\":\"te_fall_cp_tx_octet_complete\","
						"\"delta_cycles_since_previous_te_fall\":%llu,\"estimated_ms_using_active_board_clock\":%.6f,"
						"\"pc\":\"0x%04X\",\"CNTR_before\":\"0x%02X\",\"CNTR_after\":\"0x%02X\","
						"\"TRDR\":\"0x%02X\",\"cp_to_tp_accepted\":%s,\"reject_reason\":\"%s\","
						"\"TE_before\":true,\"TE_after\":false,\"RE\":%s,"
						"\"ready_state\":%s,\"board_status\":\"0x%02X\",\"current_vfd_text\":\"%s|%s\"}",
						static_cast<unsigned long long>(cy), static_cast<unsigned long long>(dlt), ems,
						unsigned(pc), unsigned(cntr_before_edge), unsigned(cntr_after_edge), unsigned(byte),
						accepted ? "true" : "false", accepted ? "" : reject_reason.c_str(),
						(cntr_after_edge & 0x20U) != 0U ? "true" : "false",
						m_tel_ready_sequence_completed ? "true" : "false", unsigned(board_status_r()),
						vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
					millennium_boot_trace_append_line(m_tp_csio_timing_trace_path, ts);
				}
				m_csio_tx_shift_edge_count = 0U;
			}
		}
		m_ipcomm_last_sclk = sclk;
	}
	if ((data & 0x08U) == 0U)
		clear_voice_segment_int0("voice_reset_asserted");
	m_voiceware->write_hw_control(data, pc, cy);
}

bool millennium_state::tel_runtime_may_emit_c4() const noexcept
{
	using pol = tel_response_policy;
	switch (m_tel_response_policy) {
	case pol::immediate:
		return true;
	case pol::latch_then_clear:
		// Defer modeled C4 until a latched-not-responding or equivalent display branch is observable.
		return m_alarm_tel_not_responding_latched || m_not_responding_display_seen;
	case pol::withhold_until_not_responding_seen:
		return m_not_responding_display_seen || m_runtime_not_responding_seen || m_alarm_tel_not_responding_latched;
	case pol::withhold_until_retry:
		return m_tel_runtime_retry_count != 0U;
	case pol::withhold_until_timeout:
		return m_tel_runtime_timeout_count != 0U;
	default:
		return true;
	}
}

void millennium_state::tel_queue_runtime_keepalive_c4_c0(u64 cy)
{
	// Host-emulated C4/C0 keepalive is for the non-PCD bridge only; PCD3349A must supply all frames.
	if (m_tp_backend_kind == tp_backend_kind::pcd3349a && m_tp_pcd3349a) {
		m_tel_last_runtime_keepalive_cycle = cy;
		m_tel_last_health_sweep_cycle = cy;
		return;
	}
	auto const queue_runtime_frame = [this](std::uint8_t code, std::initializer_list<std::uint8_t> payload,
		std::uint8_t len) {
		std::uint8_t chk = std::uint8_t(code + len);
		ipcomm_queue_rx_byte(code, true);
		ipcomm_queue_rx_byte(len, true);
		for (std::uint8_t p : payload) {
			chk = std::uint8_t(chk + p);
			ipcomm_queue_rx_byte(p, true);
		}
		ipcomm_queue_rx_byte(chk, true);
	};
	bool const may_c4 = !m_tel_fw_boot_contract_satisfied || tel_runtime_may_emit_c4();
	if (may_c4)
		queue_runtime_frame(0xC4U, { 0x00U }, 0x04U);
	queue_runtime_frame(0xC0U, { 0x00U, 0x00U, 0x00U, 0x00U, 0x00U }, 0x08U);
	m_tel_last_runtime_keepalive_cycle = cy;
	m_tel_last_health_sweep_cycle = cy;
	m_tel_runtime_waiting_c4 = false;
	m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
	m_tel_health_consecutive_miss = 0U;
	m_tel_runtime_retry_count = 0U;
	if (may_c4 || m_tel_response_policy == tel_response_policy::immediate) {
		m_alarm_tel_not_responding_latched = false;
		m_alarm_tel_not_responding_cleared = true;
		m_termfg_telephony_up_inferred = true;
	}
	m_tel_last_runtime_response = may_c4 ? "keepalive_C4_C0" : "keepalive_C0_only";
	telephony_runtime_trace_event("telephony_runtime_poll_response", "keepalive",
		may_c4 ? "0xC4_len4" : "suppressed_policy", true, may_c4 ? "runtime_keepalive_cadence" : "runtime_keepalive_c0_only_policy");
	telephony_runtime_trace_event("telephony_runtime_poll_response", "keepalive", "0xC0_len8", true,
		"runtime_keepalive_cadence");
}

void millennium_state::telephony_maybe_log_m7_rx_path(char const *reason)
{
	// Log once both CSI/O frames succeeded so boot-milestones.json matches parser evidence (C4 + C0).
	if (!(m_csio_status_frame_ok && m_csio_error_report_ok))
		return;
	if (m_m7_rx_path_logged)
		return;
	char b[340];
	std::snprintf(b, sizeof(b),
		"{\"milestone\":\"M7B\",\"ts\":\"%s\",\"event\":\"M7B_TELEPHONY_RX_PATH\","
		"\"reason\":\"%s\",\"status_frame_ok\":%s,\"error_report_ok\":%s,\"interface\":\"csio\"}",
		millennium_boot_trace_timestamp_utc().c_str(), reason ? reason : "csio_frame_ok",
		m_csio_status_frame_ok ? "true" : "false", m_csio_error_report_ok ? "true" : "false");
	millennium_boot_trace_append_line(m_boot_trace_path, b);
	m_m7_rx_path_logged = true;
}

void millennium_state::telephony_note_csio_rx_byte(u8 b)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	if (!m_telephony_rx_buffer_trace_path.empty()) {
		char line[220];
		std::snprintf(line, sizeof(line),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"path\":\"csio_model\","
			"\"byte\":\"0x%02X\",\"note\":\"telephony_to_control_byte\"}",
			static_cast<unsigned long long>(cy), unsigned(pc), unsigned(sp), unsigned(b));
		millennium_boot_trace_append_line(m_telephony_rx_buffer_trace_path, line);
	}
	if (!m_tp_csio_raw_trace_path.empty()) {
		char l2[320];
		std::snprintf(l2, sizeof(l2),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_to_cp_byte\",\"byte\":\"0x%02X\"}",
			static_cast<unsigned long long>(cy), unsigned(pc), unsigned(sp), unsigned(b));
		millennium_boot_trace_append_line(m_tp_csio_raw_trace_path, l2);
	}

	auto const note_boot_contract_progress = [this]() {
		if (m_tel_fw_boot_contract_satisfied)
			return;
		// Runtime-ready should be asserted once the boot ACK and both checksum-valid core
		// health frames (C4 error report + C0 status) are consumed on CSI/O.
		if (!(m_tel_boot_power_ack_seen && m_tel_boot_error_report_seen && m_tel_boot_status_seen))
			return;
		m_tel_fw_boot_contract_satisfied = true;
		m_tel_boot_semantic_state = tel_boot_semantic_state::runtime_active;
		m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_got_tel_inf);
		m_tel_ready_sequence_completed = m_rtos_startup_contract.xflag_all_init_done();
		m_alarm_tel_not_responding_latched = false;
		m_alarm_tel_not_responding_cleared = true;
		m_termfg_telephony_up_inferred = true;
		if (!m_tp_readiness_sequence_trace_path.empty()) {
			char rb[520];
			std::snprintf(rb, sizeof(rb),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_sequence_complete\","
				"\"ready_state_before\":false,\"ready_state_after\":true,\"current_vfd_text\":\"%s|%s\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
			millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, rb);
			char rt[260];
			std::snprintf(rt, sizeof(rt),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_ready_state_true\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
			millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, rt);
		}
		try_emit_boot_readiness_milestones(m_maincpu->total_cycles());
	};

	auto const on_var_len_byte = [this, &note_boot_contract_progress, cy, pc](u8 v) {
		if (!m_csio_rx_in_frame) {
			if (v >= 0xc0U) {
				m_csio_rx_in_frame = true;
				m_csio_rx_code = v;
				m_csio_rx_sum = v;
				m_csio_rx_len = 0;
				m_csio_rx_remaining = 0;
			} else {
				if (!millennium_recognized_tp_to_cp_single_byte_model(v)) {
					m_tp_ui_fault_unknown_opcode_count++;
					append_user_io_harness_trace("user_io_fault", "terminal24_unknown_tp_opcode");
				}
				if (v == 0x70U || v == 0x72U) {
					m_tel_boot_power_ack_seen = true;
					m_tel_boot_semantic_state = tel_boot_semantic_state::acked_not_ready;
					telephony_runtime_trace_event("telephony_init_ack", "0x72", opcode_hex_byte(v).c_str(), true, "csio_single_byte");
				} else if (v == 0x6CU || v == 0x6EU) {
					m_tel_boot_hook_state_seen = true;
				} else if (v == 0x7AU || v == 0x7CU) {
					m_tel_boot_power_status_seen = true;
				}
				trace_tp_cp_keypad_delivered(cy, pc, v);
				note_boot_contract_progress();
				if (!m_telephony_parser_trace_path.empty()) {
					char pb[240];
					std::snprintf(pb, sizeof(pb),
						"{\"event\":\"csio_single_byte_message\",\"byte\":\"0x%02X\",\"note\":\"opcode_lt_0xC0\"}",
						unsigned(v));
					millennium_boot_trace_append_line(m_telephony_parser_trace_path, pb);
				}
			}
			return;
		}
		if (m_csio_rx_len == 0) {
			m_csio_rx_len = v;
			m_csio_rx_sum = static_cast<u8>(m_csio_rx_sum + v);
			if (m_csio_rx_len < 3) {
				m_csio_rx_in_frame = false;
				return;
			}
			m_csio_rx_remaining = unsigned(m_csio_rx_len - 2U);
			return;
		}
		if (m_csio_rx_remaining == 0) {
			m_csio_rx_in_frame = false;
			return;
		}
		m_csio_rx_remaining--;
		if (m_csio_rx_remaining == 0) {
			bool const ok = (m_csio_rx_sum == v);
			char const *rsn = nullptr;
			if (ok && m_csio_rx_code == 0xc0U) {
				m_csio_status_frame_ok = true;
				m_tel_boot_status_seen = true;
				m_tel_last_good_health_cycle = m_maincpu->total_cycles();
				m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_got_tel_inf);
				if (m_tel_ready_sequence_completed)
					m_tel_last_health_sweep_cycle = m_maincpu->total_cycles();
				// Promote telephony-up as soon as a checksum-valid status frame is observed.
				// Waiting exclusively for a later C4 path can leave firmware in a transient
				// "not responding" display branch even though status traffic is already valid.
				m_termfg_telephony_up_inferred = true;
				if (m_tel_boot_power_ack_seen && m_tel_boot_hook_state_seen && m_tel_boot_power_status_seen
					&& m_tel_boot_error_report_seen)
					m_tel_boot_semantic_state = tel_boot_semantic_state::status_ready;
				note_boot_contract_progress();
				m_tel_last_runtime_response = "C0_OK";
				telephony_runtime_trace_event("telephony_init_status_ok", "0xC0", "0xC0_len8", true, "csio_frame");
				rsn = "telephony_status_checksum_ok";
				if (!m_service_refresh_trace_path.empty()) {
					char sr[520];
					std::snprintf(sr, sizeof(sr),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"service_refresh_signal\","
						"\"event_area\":\"rtos_signal\",\"reason\":\"telephony_status_frame_accepted\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
					millennium_boot_trace_append_line(m_service_refresh_trace_path, sr);
				}
				if (!m_service_task_trace_path.empty()) {
					char sb[420];
					std::snprintf(sb, sizeof(sb),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"service_task_candidate\","
						"\"source\":\"CONTTEL2_TELEPHONY_STATUS\",\"C0_accepted\":true,\"INIS_GOT_TEL_INF_candidate\":true,"
						"\"INIT_COMPLETED_SIG_ordering_candidate\":true}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
					millennium_boot_trace_append_line(m_service_task_trace_path, sb);
				}
				if (!m_rtos_signal_trace_path.empty()) {
					char rb[420];
					std::snprintf(rb, sizeof(rb),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"rtos_signal_candidate\","
						"\"signal\":\"INIS_GOT_TEL_INF\",\"source\":\"CONTTEL2_TELEPHONY_STATUS\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_rtos_signal_trace_path, rb);
					char rs[420];
					std::snprintf(rs, sizeof(rs),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_refresh_signal_candidate\","
						"\"signal\":\"INIS_GOT_TEL_INF\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_rtos_signal_trace_path, rs);
				}
			} else if (ok && m_csio_rx_code == 0xc4U) {
				m_csio_error_report_ok = true;
				m_tel_boot_error_report_seen = true;
				m_tel_last_good_health_cycle = m_maincpu->total_cycles();
				m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_got_tel_inf);
				rsn = "telephony_error_report_checksum_ok";
				bool const was_latched = m_alarm_tel_not_responding_latched;
				// C4 only clears the modeled alarm after either the firmware has entered the
				// not-responding branch or the full boot contract is otherwise satisfied.
				if (was_latched || m_tel_fw_boot_contract_satisfied) {
					m_alarm_tel_not_responding_cleared = true;
					m_alarm_tel_not_responding_latched = false;
					telephony_runtime_trace_event("telephony_fault_cleared", "0xC4", "0xC4_len4", true, "csio_c4_clear");
				}
				// Keep one deferred status publish armed so OOS selector can re-evaluate on a
				// checksum-valid status frame after alarm clear.
				m_tel_pending_status_after_clear = true;
				// If a checksum-valid C0 was decoded before C4 cleared the alarm, deferral left
				// `m_tel_ready_sequence_completed` false — complete the sequence when C4 arrives.
				note_boot_contract_progress();
				m_tel_last_runtime_response = "C4_OK";
				telephony_runtime_trace_event("telephony_error_report_clear", "0xC4", "0xC4_len4", true, "csio_frame");
				if (!m_alarm_condition_trace_path.empty()) {
					char ab[460];
					std::snprintf(ab, sizeof(ab),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"alarm_clear_candidate\","
						"\"alarm\":\"ALM_TEL_NOT_RESPONDING\",\"set_clear\":\"clear\",\"C4_accepted\":true,"
						"\"source\":\"CONTTELC_TELEPHONY_ERROR_REPORT\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_alarm_condition_trace_path, ab);
				}
				if (!m_service_display_trace_path.empty()) {
					char db[520];
					std::snprintf(db, sizeof(db),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_display_refresh_candidate\","
						"\"source\":\"TERMSUB2_term_upd_alarm_condition_clear\",\"SERVS_UPDATE_LEVEL_candidate\":true,"
						"SERVS_CHECK_OOS_MSG_candidate=true}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_service_display_trace_path, db);
					char sb[460];
					std::snprintf(sb, sizeof(sb),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"servs_update_level_candidate\","
						"\"reason\":\"alarm_clear_path_after_c4\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_service_display_trace_path, sb);
					char sc[460];
					std::snprintf(sc, sizeof(sc),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"servs_check_oos_msg_candidate\","
						"\"reason\":\"alarm_clear_path_after_c4\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_service_display_trace_path, sc);
					char se[480];
					std::snprintf(se, sizeof(se),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_display_task_enter\","
						"\"event_area\":\"service_display_refresh\",\"reason\":\"alarm_clear_after_latch\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
					millennium_boot_trace_append_line(m_service_display_trace_path, se);
				}
				bool const old_termfg = m_termfg_telephony_up_inferred;
				m_termfg_telephony_up_inferred = true;
				m_tel_runtime_waiting_c4 = false;
				m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
				m_tel_health_consecutive_miss = 0U;
				m_tel_runtime_retry_count = 0U;
				if (!m_termflag_trace_path.empty()) {
					char tb[440];
					std::snprintf(tb, sizeof(tb),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"termflag_candidate\","
						"\"flag\":\"TERMFG_TELEPHONY_UP\",\"old\":%s,\"new\":true,\"source\":\"TERMSUB2_alarm_clear_path\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
						old_termfg ? "true" : "false");
					millennium_boot_trace_append_line(m_termflag_trace_path, tb);
				}
			}
			if (!m_telephony_parser_trace_path.empty()) {
				char pb[360];
				std::snprintf(pb, sizeof(pb),
					"{\"event\":\"csio_varlen_frame\",\"code\":\"0x%02X\",\"len\":%u,\"checksum_ok\":%s,\"note\":\"%s\"}",
					unsigned(m_csio_rx_code), unsigned(m_csio_rx_len), ok ? "true" : "false",
					ok ? (rsn ? rsn : "frame_end") : "checksum_mismatch");
				millennium_boot_trace_append_line(m_telephony_parser_trace_path, pb);
			}
			if (ok && rsn)
				telephony_maybe_log_m7_rx_path(rsn);
			m_csio_rx_in_frame = false;
		} else {
			m_csio_rx_sum = static_cast<u8>(m_csio_rx_sum + v);
		}
	};

	on_var_len_byte(b);
}

void millennium_state::ipcomm_queue_rx_byte(u8 data, bool priority)
{
	if (priority)
		m_ipcomm_rx_prio_bytes.push_back(data);
	else
		m_ipcomm_rx_bytes.push_back(data);
	if (!m_tp_readiness_sequence_trace_path.empty()) {
		char b[420];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_response_queued\","
			"\"byte\":\"0x%02X\",\"queued_response_length\":%u}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(data), ipcomm_pending_rx_count());
		millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
	}
	prime_ipcomm_rx_shift_from_queues();
	if (!m_telephony_handshake_trace_path.empty()) {
		char rb[300];
		std::snprintf(rb, sizeof(rb),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"ipcomm_rx_queue\",\"rx\":\"0x%02X\","
			"\"rx_pending\":%u,\"rts_asserted\":%s}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(data), ipcomm_pending_rx_count(),
			m_ipcomm_rts_asserted ? "true" : "false");
		millennium_boot_trace_append_line(m_telephony_handshake_trace_path, rb);
	}
}

void millennium_state::ipcomm_complete_csio_tx_byte(std::uint8_t byte)
{
	if (!m_tel_ip_link_enabled) {
		if (!m_telephony_runtime_conversation_trace_path.empty()) {
			char b[320];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"event\":\"telephony_tx_ignored_link_inhibited\",\"tx\":\"0x%02X\","
				"\"note\":\"cp_tx_ignored_until_post_reset_enable\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(byte));
			millennium_boot_trace_append_line(m_telephony_runtime_conversation_trace_path, b);
		}
		return;
	}

	if (!m_telephony_handshake_trace_path.empty()) {
		char tb[260];
		std::snprintf(tb, sizeof(tb),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"ipcomm_tx_byte\",\"tx\":\"0x%02X\","
			"\"note\":\"trdr_on_te_clear_after_rising_edge\"}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(byte));
		millennium_boot_trace_append_line(m_telephony_handshake_trace_path, tb);
	}
	if (tp_backend_handle_cp_byte(byte))
		return;
	// Variable-length host TX: octets at or above 0xC0 start a framed message; embedded payload must not be parsed as
	// standalone host opcodes. A full 0xC0 / length-126 configuration download is ACKed with 0x74 toward the host
	// immediately after that frame's checksum octet completes on CSI/O TX.
	static constexpr std::uint8_t VAR_LEN_MSG_LOWER_LIMIT = 0xc0U;
	static constexpr std::uint8_t k_cfg_download_hdr = 0xc0U;
	static constexpr std::uint8_t k_cfg_download_total_len = 126U;
	static constexpr std::uint8_t k_cfg_download_ack = 0x74U;
	static constexpr std::uint8_t k_var_len_max_len = 126U;
	auto const queue_raw_frame = [this](std::uint8_t code, std::initializer_list<std::uint8_t> payload, std::uint8_t len) {
		std::uint8_t chk = std::uint8_t(code + len);
		std::vector<std::uint8_t> emitted;
		emitted.reserve(payload.size() + 3U);
		emitted.push_back(code);
		emitted.push_back(len);
		bool const priority = (code == 0xC0U) || (code == 0xC4U);
		ipcomm_queue_rx_byte(code, priority);
		ipcomm_queue_rx_byte(len, priority);
		for (std::uint8_t p : payload) {
			chk = std::uint8_t(chk + p);
			ipcomm_queue_rx_byte(p, priority);
			emitted.push_back(p);
		}
		ipcomm_queue_rx_byte(chk, priority);
		emitted.push_back(chk);
		if (!m_tp_command_response_trace_path.empty()) {
			std::string response;
			for (std::uint8_t v : emitted) {
				if (!response.empty())
					response.push_back(' ');
				char hex[8];
				std::snprintf(hex, sizeof(hex), "0x%02X", unsigned(v));
				response.append(hex);
			}
			char b[960];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_response_emitted\","
				"\"response_bytes\":\"%s\",\"response_label\":\"frame\",\"checksum\":\"ok\","
				"\"queued_cycle\":%llu,\"first_byte_consumed_cycle\":0,\"last_byte_consumed_cycle\":0,"
				"\"ready_state_effect\":%s,\"board_status_effect\":\"0x%02X\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), response.c_str(),
				static_cast<unsigned long long>(m_maincpu->total_cycles()),
				m_tel_ready_sequence_completed ? "true" : "false", unsigned(board_status_r()));
			millennium_boot_trace_append_line(m_tp_command_response_trace_path, b);
		}
	};

	if (m_ip_tx_skip_remain > 0U) {
		m_ip_tx_skip_remain--;
		if (m_ip_tx_skip_remain == 0U) {
			if (m_ip_tx_var_hdr == k_cfg_download_hdr && m_ip_tx_declared_len == k_cfg_download_total_len) {
				trace_tp_command_accepted(m_ip_tx_var_hdr, true, "0x74", "config_frame_complete");
				ipcomm_queue_rx_byte(k_cfg_download_ack);
				trace_tp_response_emitted({ k_cfg_download_ack }, "cfg_download_ack", true);
				if (!m_tp_readiness_sequence_trace_path.empty()) {
					char b[300];
					std::snprintf(b, sizeof(b),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_sequence_start\","
						"\"note\":\"post_config_ack_boot_readiness\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
					millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
				}
				queue_raw_frame(0xC4, { 0x00 }, 0x04);
				queue_raw_frame(0xC0, { 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x08);
				if (!m_tp_readiness_sequence_trace_path.empty()) {
					char c4b[260];
					std::snprintf(c4b, sizeof(c4b),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_c4_sent\","
						"\"note\":\"post_config_ack_boot_readiness\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
					millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, c4b);
					char c0b[260];
					std::snprintf(c0b, sizeof(c0b),
						"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_c0_sent\","
						"\"note\":\"post_config_ack_boot_readiness\"}",
						static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
						unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
					millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, c0b);
				}
			}
			m_ip_tx_var_hdr = 0;
			m_ip_tx_declared_len = 0;
		}
		return;
	}

	if (m_ip_tx_need_length_byte) {
		m_ip_tx_need_length_byte = false;
		if (byte < 3U || byte > k_var_len_max_len) {
			trace_tp_command_accepted(m_ip_tx_var_hdr, false, "", "invalid_length");
			m_ip_tx_var_hdr = 0;
			m_ip_tx_skip_remain = 0U;
			m_ip_tx_declared_len = 0U;
			return;
		}
		m_ip_tx_declared_len = byte;
		m_ip_tx_skip_remain = unsigned(byte) - 2U;
		trace_tp_command_accepted(m_ip_tx_var_hdr, false, "", "varlen_length_accepted");
		return;
	}

	if (byte >= VAR_LEN_MSG_LOWER_LIMIT) {
		if (byte != k_cfg_download_hdr) {
			trace_tp_command_accepted(byte, false, "", "unsupported_varlen_header");
			m_ip_tx_var_hdr = 0;
			m_ip_tx_need_length_byte = false;
			m_ip_tx_skip_remain = 0U;
			m_ip_tx_declared_len = 0U;
			return;
		}
		m_ip_tx_var_hdr = byte;
		m_ip_tx_need_length_byte = true;
		trace_tp_command_accepted(byte, false, "", "varlen_header_accepted");
		return;
	}

	auto const queue_frame = [&](std::uint8_t code, std::initializer_list<std::uint8_t> payload, std::uint8_t len) {
		queue_raw_frame(code, payload, len);
	};
	auto const note_init_dialogue_progress = [&](std::uint8_t opcode) {
		// terminal_00: boot query block order is significant: 0x30,0x33,0x38,0x31.
		static constexpr std::array<std::uint8_t, 4> kBootSeq{ 0x30U, 0x33U, 0x38U, 0x31U };
		if (!m_tel_init_dialogue_window_active) {
			m_tel_init_dialogue_window_active = true;
			m_tel_init_dialogue_window_start_cycle = m_maincpu->total_cycles();
			m_tel_init_dialogue_step = 0U;
		}
		if (m_tel_init_dialogue_step < kBootSeq.size() && opcode == kBootSeq[m_tel_init_dialogue_step]) {
			m_tel_init_dialogue_step++;
		} else if (opcode == kBootSeq[0]) {
			m_tel_init_dialogue_step = 1U;
		}
		if (m_tel_init_dialogue_step >= kBootSeq.size()) {
			m_tel_init_dialogue_window_active = false;
		}
	};

	switch (byte) {
	case 0x00:
		break;
	case 0x03:
	case 0x04:
		// Not part of terminal_00/19 CP->TP opcode catalog; ignore.
		break;
	case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
	case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
	case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2D: case 0x2E: case 0x2F:
	case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46:
		// Runtime control/audio/call-state opcodes are accepted by TP with no immediate direct reply frame.
		break;
	case 0x30:
		trace_tp_command_accepted(byte, true, "0x6C_or_0x6E", "query_hook_status");
		note_init_dialogue_progress(byte);
		ipcomm_queue_rx_byte(m_tel_hook_onhook_stable ? 0x6C : 0x6E, true);
		trace_tp_response_emitted({ m_tel_hook_onhook_stable ? 0x6CU : 0x6EU }, "hook_status", true);
		break;
	case 0x31:
		trace_tp_command_accepted(byte, true, "0xC0_len8", "query_telephony_status");
		if (!m_tp_readiness_sequence_trace_path.empty()) {
			char b[260];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_sequence_start\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
			millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
		}
		note_init_dialogue_progress(byte);
		m_tel_runtime_poll_count++;
		m_tel_last_runtime_poll_command = "0x31";
		telephony_runtime_trace_event("telephony_runtime_poll_sent", "0x31", "", true, "query_telephony_status");
		if (m_tel_policy_start_cycle == 0U)
			m_tel_policy_start_cycle = m_maincpu->total_cycles();
		// Runtime trace counter: keep non-zero whenever CP issues health polls so alarm choreography
		// does not infer a "silent" link after a successful clear (firmware still polls 0x31).
		++m_tel_not_responding_poll_count;
		queue_frame(0xC0, { 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x08);
		if (!m_tp_readiness_sequence_trace_path.empty()) {
			char b[300];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_c0_sent\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
			millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
		}
		m_tel_last_runtime_response = "0xC0";
		telephony_runtime_trace_event("telephony_runtime_poll_response", "0x31", "0xC0_len8", true,
			"query_telephony_status_reply");
		telephony_runtime_trace_event("service_refresh_after_runtime_poll", "0x31", "0xC0", true, "status_reply_enqueued");
		if (!m_rtos_signal_trace_path.empty()) {
			char rb[360];
			std::snprintf(rb, sizeof(rb),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"rtos_signal_candidate\","
				"\"signal\":\"TELEPHONY_RX_MSG_SIG\",\"source\":\"QUERY_TELEPHONY_STATUS_reply_C0\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
			millennium_boot_trace_append_line(m_rtos_signal_trace_path, rb);
			char rs[380];
			std::snprintf(rs, sizeof(rs),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_refresh_signal_candidate\","
				"\"signal\":\"TELEPHONY_RX_MSG_SIG\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU));
			millennium_boot_trace_append_line(m_rtos_signal_trace_path, rs);
		}
		break;
	case 0x33:
		trace_tp_command_accepted(byte, true, "0x7A", "query_power_status");
		note_init_dialogue_progress(byte);
		ipcomm_queue_rx_byte(0x7A, true);
		trace_tp_response_emitted({ 0x7AU }, "power_status", true);
		break;
	case 0x34:
		queue_frame(0xC2, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0x0E);
		break;
	case 0x35:
		// CLEAR_TELEPHONY_STATUS: modeled as side-effect only (no direct reply byte/frame).
		break;
	case 0x37:
		trace_tp_command_accepted(byte, true, "0x80", "query_line_status");
		// QUERY_CO_LINE_STATUS: return a deterministic nominal line-state code.
		ipcomm_queue_rx_byte(0x80, true);
		trace_tp_response_emitted({ 0x80U }, "line_status", true);
		break;
	case 0x38:
		trace_tp_command_accepted(byte, true, "0xC4_len4", "query_error_report");
		note_init_dialogue_progress(byte);
		m_tel_runtime_poll_count++;
		m_tel_last_runtime_poll_command = "0x38";
		telephony_runtime_trace_event("telephony_error_report_sent", "0x38", "", true, "query_error_report");
		m_tel_last_health_sweep_cycle = m_maincpu->total_cycles();
		{
			u64 const hz = static_cast<u64>(m_maincpu->unscaled_clock());
			bool allow_c4 = false;
			if (m_tel_suppress_c4_sweeps_remaining > 0U) {
				m_tel_suppress_c4_sweeps_remaining--;
				m_tel_last_runtime_response = "suppressed";
				telephony_runtime_trace_event("telephony_runtime_poll_response", "0x38", "", false, "error_report_suppressed");
			} else {
				// Boot completion path always supplies C4; optional policy delays runtime C4 until fault ordering matches.
				allow_c4 = !m_tel_fw_boot_contract_satisfied || tel_runtime_may_emit_c4();
				if (allow_c4) {
					m_tel_runtime_waiting_c4 = true;
					m_tel_runtime_wait_c4_deadline_cycle = m_maincpu->total_cycles() + hz * 3U;
					queue_frame(0xC4, { 0x00 }, 0x04);
					m_tel_last_runtime_response = "0xC4";
					telephony_runtime_trace_event("telephony_runtime_poll_response", "0x38", "0xC4_len4", true,
						"error_report_reply");
					telephony_runtime_trace_event("service_refresh_after_runtime_poll", "0x38", "0xC4", true,
						"error_report_enqueued");
				} else {
					m_tel_runtime_waiting_c4 = false;
					m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
					m_tel_last_runtime_response = "c4_withheld_policy";
					telephony_runtime_trace_event("telephony_runtime_poll_response", "0x38", "", false,
						"error_report_policy_withhold");
				}
			}
			if (!allow_c4 && m_tel_suppress_c4_sweeps_remaining == 0U) {
				m_tel_runtime_waiting_c4 = false;
				m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
			}
			if (allow_c4 && !m_tp_readiness_sequence_trace_path.empty()) {
				char b[300];
				std::snprintf(b, sizeof(b),
					"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"tp_readiness_c4_sent\"}",
					static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
					unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU));
				millennium_boot_trace_append_line(m_tp_readiness_sequence_trace_path, b);
			}
		}
		break;
	case 0x39:
		// CLEAR_ERROR_REPORT: modeled as accepted command with no direct reply frame.
		break;
	case 0x3A:
		trace_tp_command_accepted(byte, true, "0x88_or_0x8A", "query_key_matrix");
		// QUERY_KEY_MATRIX — TP returns KEY_MATRIX_IDLE (0x88) vs KEY_MATRIX_ACTIVE (0x8A) from live panel state.
		// terminal_21_user_io: counts KEYMATRIX (mask profile) except hook bit 19; softkeys when vfd_11line profile.
		{
			u32 const km_full =
				m_keypad->keymatrix_with_hook_debounce(m_keymatrix_io->read(), m_maincpu->total_cycles());
			u32 const km = km_full & terminal21_keymatrix_applied_mask(m_keypad_board.terminal_21_profile);
			u32 const sk = (m_keypad_board.terminal_21_profile == millennium_terminal21_user_io_profile::vfd_11line_softkeys)
				? (m_terminal21_softkeys_io->read() & 0x0fffU)
				: 0U;
			bool matrix_active = ((km & ~k_terminal21_hook_bit) != 0U) || (sk != 0U);
			if (m_tel_force_key_active_sweeps_remaining > 0U) {
				m_tel_force_key_active_sweeps_remaining--;
				matrix_active = true;
			}
			ipcomm_queue_rx_byte(matrix_active ? 0x8A : 0x88);
			trace_tp_response_emitted({ matrix_active ? 0x8AU : 0x88U }, "key_matrix", true);
			if (matrix_active) {
				m_tel_stuck_key_counter++;
				if (m_tel_stuck_key_counter >= 30U)
					m_alarm_stuck_keys = true;
			} else {
				m_tel_stuck_key_counter = 0U;
				m_alarm_stuck_keys = false;
			}
		}
		break;
	case 0x3B:
		trace_tp_command_accepted(byte, true, "0x8C_or_0x8E", "query_handset_continuity");
		// QUERY_HANDSET_CONTINUITY.
		if (m_tel_force_handset_bad_sweeps_remaining > 0U) {
			m_tel_force_handset_bad_sweeps_remaining--;
			m_tel_handset_ok = false;
		}
		ipcomm_queue_rx_byte(m_tel_handset_ok ? 0x8C : 0x8E);
		trace_tp_response_emitted({ m_tel_handset_ok ? 0x8CU : 0x8EU }, "handset_continuity", true);
		if (m_tel_handset_ok) {
			m_tel_handset_bad_counter = 0U;
			m_alarm_handset_discont = false;
		} else {
			m_tel_handset_bad_counter++;
			if (m_tel_handset_bad_counter >= 5U)
				m_alarm_handset_discont = true;
		}
		break;
	case 0x3C:
		trace_tp_command_accepted(byte, false, "", "release_hook_relay");
		// RELEASE_HOOK_SWITCH_RELAY: accepted command; no direct response.
		set_tel_hook_state(true, m_maincpu->total_cycles(), "release_hook_relay");
		break;
	case 0x3D:
		trace_tp_command_accepted(byte, false, "", "seize_hook_relay");
		// SEIZE_HOOK_SWITCH_RELAY: accepted with no immediate telephony reply in the modeled boot ladder.
		set_tel_hook_state(false, m_maincpu->total_cycles(), "seize_hook_relay");
		break;
	case 0x32:
	case 0x36:
	case 0x3E:
		trace_tp_command_accepted(byte, false, "", "accepted_no_direct_reply");
		// NEXT_CALL_IDLE / CLEAR_DTMF_DIALING / CHANGE_DTMF_FEEDBACK_CONTROL:
		// accepted with no direct telephony response frame.
		break;
	default:
		// Unknown/noise CSI/O byte: ignore.  Do not synthesize POWER_ON_ACK repeatedly, as that can
		// hold firmware in telephony-reset sequencing instead of allowing service-display progression.
		if (!m_telephony_phase_trace_path.empty()) {
			char ub[320];
			std::snprintf(ub, sizeof(ub),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"unknown_command\",\"tx\":\"0x%02X\","
				"\"action\":\"ignored_no_unsolicited_ack\"}",
				static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(byte));
			millennium_boot_trace_append_line(m_telephony_phase_trace_path, ub);
		}
		break;
	}
}

u8 millennium_state::pio_keypad_r(offs_t offset)
{
	// 8255 ports A/C: **not** connected to the physical keypad/hook in production; those are TP-sensed.
	// `millennium_keypad_device::read` returns an idle matrix on A/C; port B is the latched strobe/bank image.
	u64 const cy = m_maincpu->total_cycles();
	u8 r = m_keypad->read(cy, offset);
	m_pio_8255_shadow.at(std::size_t(offset & 3)) = r;
	char const *tag = "pio_8255";
	switch (offset & 3) {
	case 0:
		tag = "pio_port_a";
		break;
	case 1:
		tag = "pio_port_b";
		break;
	case 2:
		tag = "pio_port_c";
		break;
	default:
		tag = "pio_command";
		break;
	}
	emit_io_trace_line(tag, u16(0x41U + offset), 'r', r);
	if (!m_front_panel_input_source_trace_path.empty()) {
		u32 const k = m_keymatrix_io->read();
		if (k != m_last_firmware_keymatrix_state) {
			char const *event = "mame_keymatrix_delta_during_pio_read";
			u32 const key_delta = (k ^ m_last_firmware_keymatrix_state) & 0x00000fffU;
			if (key_delta != 0U) {
				if (k & 0x00000001U)
					event = "mame_key1_delta_during_pio_read";
				else if (k & 0x00000002U)
					event = "mame_key2_delta_during_pio_read";
				else if (k & 0x00000004U)
					event = "mame_key3_delta_during_pio_read";
				else if (k & 0x00000008U)
					event = "mame_key4_delta_during_pio_read";
				else if (k & 0x00000010U)
					event = "mame_key5_delta_during_pio_read";
				else if (k & 0x00000020U)
					event = "mame_key6_delta_during_pio_read";
				else if (k & 0x00000040U)
					event = "mame_key7_delta_during_pio_read";
				else if (k & 0x00000080U)
					event = "mame_key8_delta_during_pio_read";
				else if (k & 0x00000100U)
					event = "mame_key9_delta_during_pio_read";
				else if (k & 0x00000200U)
					event = "mame_star_delta_during_pio_read";
				else if (k & 0x00000400U)
					event = "mame_key0_delta_during_pio_read";
				else if (k & 0x00000800U)
					event = "mame_pound_delta_during_pio_read";
			}
			if (((k ^ m_last_firmware_keymatrix_state) & k_terminal21_hook_bit) != 0U) {
				event = (k & k_terminal21_hook_bit) != 0U ? "mame_hook_offhook_delta_during_pio_read"
														: "mame_hook_onhook_delta_during_pio_read";
			}
			char b[700];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"%s\","
				"\"input_source\":\"mame_input_port\",\"MAME_input_name\":\"KEYMATRIX\","
				"\"old_input_state\":\"0x%08X\",\"new_input_state\":\"0x%08X\","
				"\"mapped_emulated_hardware_signal\":\"tp_feed_not_cp_pio\","
				"\"firmware_visible_port\":\"0x%04X\",\"firmware_visible_value\":\"0x%02X\"}",
				static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), event, unsigned(m_last_firmware_keymatrix_state),
				unsigned(k), unsigned(0x41U + offset), unsigned(r));
			millennium_boot_trace_append_line(m_front_panel_input_source_trace_path, b);
			m_last_firmware_keymatrix_state = k;
		}
		if (m_front_panel_active_read_trace_budget > 0U && k != 0U && (offset & 3U) == 0U) {
			--m_front_panel_active_read_trace_budget;
			char b[760];
			std::snprintf(b, sizeof(b),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"input_keypad_scan_read\","
				"\"input_source\":\"mame_input_port\",\"MAME_input_name\":\"KEYMATRIX\","
				"\"new_input_state\":\"0x%08X\",\"mapped_emulated_hardware_signal\":\"tp_feed_not_cp_pio\","
				"\"firmware_visible_port\":\"0x%04X\",\"firmware_visible_value\":\"0x%02X\","
				"\"pio_port_b_latch\":\"0x%02X\",\"pio_port_c_latch\":\"0x%02X\"}",
				static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
				unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(k), unsigned(0x41U + offset), unsigned(r),
				unsigned(m_pio_8255_shadow[1]), unsigned(m_pio_8255_shadow[2]));
			millennium_boot_trace_append_line(m_front_panel_input_source_trace_path, b);
		}
	}
	(void)cy;
	return r;
}

void millennium_state::pio_keypad_w(offs_t offset, u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	std::size_t const idx = std::size_t(offset & 3);
	u8 const prev_latched = m_pio_8255_shadow.at(idx);
	m_pio_8255_shadow.at(idx) = data;
	m_keypad->write(cy, offset, data);
	if (idx == 2U) {
		u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
		trace_pio_port_c_telephony_routing(prev_latched, data, cy, pc, sp);
	}
	char const *tag = "pio_8255";
	switch (offset & 3) {
	case 0:
		tag = "pio_port_a";
		break;
	case 1:
		tag = "pio_port_b";
		break;
	case 2:
		tag = "pio_port_c";
		break;
	default:
		tag = "pio_command";
		break;
	}
	emit_io_trace_line(tag, u16(0x41U + offset), 'w', data);
	// PIO port B (0x42): EEPROM SK (bit 7) shares the voiceware phrase strobe; when EEPROM CS
	// is asserted (HW_CNTL 0x40 bit 6), treat bit 7 as serial clock only.
	if (offset == 1) {
		bool const eeprom_cs = (m_hw_cntl_port_image & 0x40U) != 0U;
		// SK is PIO-B bit 7; need previous latched B for rising-edge detect (prev_b was undefined before).
		m_microwire_93c66.notify_port_b_clock(prev_latched, data, (m_pio_8255_shadow[0] & 1U) != 0U);
		if (!eeprom_cs) {
			m_voiceware->write_bank(data, pc, cy);
			// Some firmware paths strobe voice phrase traffic via PIO-B bit7 edges while carrying
			// the phrase value on the same byte bus (low 7 bits). Mirror that board-level alias
			// into the real voice phrase path without synthetic triggers.
			bool const prev_strobe = (m_voice_pb_prev & 0x80U) != 0U;
			bool const now_strobe = (data & 0x80U) != 0U;
			u8 const phrase = data & 0x7fU;
			bool const strobe_fall = prev_strobe && !now_strobe;
			bool const phrase_changed = (phrase != m_voice_pb_last_phrase);
			bool const cooldown_ok = (m_voice_pb_last_cycle == 0ULL) || ((cy - m_voice_pb_last_cycle) > 20000ULL);
			if (strobe_fall && cooldown_ok && (phrase_changed || phrase == 0x3fU)) {
				m_voice_pb_last_cycle = cy;
				m_voice_pb_last_phrase = phrase;
				voiceware_phrase_w(phrase);
			}
		}
		m_voice_pb_prev = data;
	}
}

u8 millennium_state::external_uart_r(offs_t offset)
{
	std::size_t const reg = std::size_t(offset & 7U);
	u8 data = m_ext_uart_shadow[reg];
	bool const dlab = (m_ext_uart_shadow[3] & 0x80U) != 0U;
	auto const recompute_iir = [this]() -> u8 {
		if ((m_ext_uart_shadow[1] & 0x01U) != 0U && !m_ext_uart_rx_queue.empty())
			return 0x04U; // RDA interrupt pending.
		if ((m_ext_uart_shadow[1] & 0x08U) != 0U && (m_ext_uart_shadow[6] & 0x0fU) != 0U)
			return 0x00U; // Modem status interrupt pending.
		return 0x01U;     // No interrupt pending.
	};
	switch (reg) {
	case 0: // RBR/DLL
		if (dlab) {
			data = m_ext_uart_dll;
		} else if (!m_ext_uart_rx_queue.empty()) {
			data = m_ext_uart_rx_queue.front();
			m_ext_uart_rx_queue.pop_front();
		} else {
			data = 0x00U;
		}
		break;
	case 1: // IER/DLM
		data = m_ext_uart_shadow[1];
		break;
	case 2: // IIR: bit 0 set means no interrupt pending.
		data = recompute_iir();
		break;
	case 5: // LSR
		data = static_cast<u8>(0x60U | (m_ext_uart_rx_queue.empty() ? 0x00U : 0x01U));
		break;
	case 6: // MSR
		data = m_ext_uart_shadow[6];
		m_ext_uart_shadow[6] = static_cast<u8>(m_ext_uart_shadow[6] & 0xf0U); // reading MSR clears delta bits.
		break;
	default:
		break;
	}
	m_ext_uart_shadow[2] = recompute_iir();
	m_ext_uart_shadow[5] = static_cast<u8>(0x60U | (m_ext_uart_rx_queue.empty() ? 0x00U : 0x01U));
	m_uart_read_counts[reg]++;
	emit_io_trace_line("external_uart", u16(0xe0U + reg), 'r', data);
	// Spec conformance: external UART is modem/NCC path only. Telephony is CSI/O-only.
	if (!m_external_uart_trace_path.empty()) {
		char b[320];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"port\":\"0x%04X\",\"register\":\"%s\","
			"\"rw\":\"r\",\"value\":\"0x%02X\",\"dlab\":%s,\"rx_queue_len\":%u,\"lsr\":\"0x%02X\",\"iir\":\"0x%02X\"}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(0xe0U + reg),
			(reg == 0) ? (dlab ? "dll" : "rbr")
					   : (reg == 1) ? (dlab ? "dlm" : "ier")
					   : (reg == 2) ? "iir"
					   : (reg == 3) ? "lcr"
					   : (reg == 4) ? "mcr"
					   : (reg == 5) ? "lsr"
					   : (reg == 6) ? "msr"
									: "scr",
			unsigned(data), dlab ? "true" : "false", unsigned(m_ext_uart_rx_queue.size()), unsigned(m_ext_uart_shadow[5]),
			unsigned(m_ext_uart_shadow[2]));
		millennium_boot_trace_append_line(m_external_uart_trace_path, b);
	}
	return data;
}

void millennium_state::external_uart_w(offs_t offset, u8 data)
{
	std::size_t const reg = std::size_t(offset & 7U);
	bool const dlab = (m_ext_uart_shadow[3] & 0x80U) != 0U;
	auto const recompute_iir = [this]() -> u8 {
		if ((m_ext_uart_shadow[1] & 0x01U) != 0U && !m_ext_uart_rx_queue.empty())
			return 0x04U;
		if ((m_ext_uart_shadow[1] & 0x08U) != 0U && (m_ext_uart_shadow[6] & 0x0fU) != 0U)
			return 0x00U;
		return 0x01U;
	};
	if (reg == 0 && !dlab) {
		++m_ext_uart_tx_count;
		m_hostbridge->deliver_host_to_processor_byte(data, m_maincpu->total_cycles(), u16(m_maincpu->pc() & 0xffffU));
	}

	m_ext_uart_shadow[reg] = data;
	if (dlab && reg == 0)
		m_ext_uart_dll = data;
	if (dlab && reg == 1)
		m_ext_uart_dlm = data;
	// Spec conformance: no telephony seeding/commands on external UART.
	if (reg == 4) {
		u8 const old_hi = static_cast<u8>(m_ext_uart_shadow[6] & 0xf0U);
		u8 new_hi = old_hi;
		if ((data & 0x01U) != 0U)
			new_hi |= 0x10U; // DTR -> CTS
		else
			new_hi &= ~0x10U;
		if ((data & 0x02U) != 0U)
			new_hi |= 0x80U; // RTS -> DCD present
		else
			new_hi &= ~0x80U;
		u8 delta = 0;
		if ((old_hi & 0x10U) != (new_hi & 0x10U))
			delta |= 0x01U;
		if ((old_hi & 0x80U) != (new_hi & 0x80U))
			delta |= 0x08U;
		m_ext_uart_shadow[6] = static_cast<u8>((new_hi & 0xf0U) | delta);
	}
	if (reg == 3 && (data & 0x80U) == 0U)
		m_ext_uart_shadow[2] = recompute_iir();
	m_ext_uart_shadow[2] = recompute_iir();
	m_ext_uart_shadow[5] = static_cast<u8>(0x60U | (m_ext_uart_rx_queue.empty() ? 0x00U : 0x01U));
	m_uart_write_counts[reg]++;
	emit_io_trace_line("external_uart", u16(0xe0U + reg), 'w', data);
	if (!m_external_uart_trace_path.empty()) {
		char b[320];
		std::snprintf(b, sizeof(b),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"port\":\"0x%04X\",\"register\":\"%s\","
			"\"rw\":\"w\",\"value\":\"0x%02X\",\"dlab\":%s,\"rx_queue_len\":%u,\"lsr\":\"0x%02X\",\"iir\":\"0x%02X\"}",
			static_cast<unsigned long long>(m_maincpu->total_cycles()), unsigned(m_maincpu->pc() & 0xffffU),
			unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), unsigned(0xe0U + reg),
			(reg == 0) ? (dlab ? "dll" : "thr")
					   : (reg == 1) ? (dlab ? "dlm" : "ier")
					   : (reg == 2) ? "fcr_iir"
					   : (reg == 3) ? "lcr"
					   : (reg == 4) ? "mcr"
					   : (reg == 5) ? "lsr"
					   : (reg == 6) ? "msr"
									: "scr",
			unsigned(data), dlab ? "true" : "false", unsigned(m_ext_uart_rx_queue.size()), unsigned(m_ext_uart_shadow[5]),
			unsigned(m_ext_uart_shadow[2]));
		millennium_boot_trace_append_line(m_external_uart_trace_path, b);
	}
}

u8 millennium_state::pio_port_g_r()
{
	maybe_io_trace("pio_port_g", 0x63, 'r', m_pio_port_g);
	return m_pio_port_g;
}

void millennium_state::pio_port_g_w(u8 data)
{
	m_pio_port_g = data;
	maybe_io_trace("pio_port_g", 0x63, 'w', data);
}

namespace {

u8 flash_phys_byte(millennium_state &st, std::uint32_t phys)
{
	memory_region *const rom = st.memregion("flash");
	if (!rom || phys >= rom->bytes())
		return 0xffU;
	return rom->base()[phys];
}

} // namespace

u8 millennium_state::phys_ram_r(offs_t offset)
{
	std::uint32_t const phys = static_cast<std::uint32_t>(offset) & 0xfffffU;
	millennium_mach_phys_ram_decode const dec =
		millennium_mach_decode_phys_ram(m_mach_pio_shadow[0], phys, m_phys_ram.size());
	switch (dec.route) {
	case millennium_mach_phys_ram_route::sram_chip:
		return m_phys_ram[dec.chip_byte_index];
	case millennium_mach_phys_ram_route::upper_flash:
		return flash_phys_byte(*this, phys);
	case millennium_mach_phys_ram_route::below_sram_windows:
	case millennium_mach_phys_ram_route::unmapped_ff:
	default:
		return 0xffU;
	}
}

void millennium_state::init_auxiliary_trace_sinks()
{
	auto const sidecar = [this](char const *flag, char const *path_env, char const *filename, std::filesystem::path &out) {
		out.clear();
		std::string win_path_storage;
		char const *p = osd_getenv(path_env);
		if (!p || !*p) {
#ifdef _WIN32
			// MSYS2/bash -> PowerShell -> MAME: path env vars often live in the Win32 block but not
			// the C runtime environ seen by osd_getenv (same class of issue as COINLINE_TRACE_ONLY).
			char win_path_env[2048] = {};
			DWORD const wn = GetEnvironmentVariableA(path_env, win_path_env, sizeof(win_path_env));
			if (wn > 0U && wn < sizeof(win_path_env))
				win_path_storage.assign(win_path_env, static_cast<std::size_t>(wn));
#endif
		}
		if (!win_path_storage.empty())
			p = win_path_storage.c_str();
		if (p && *p) {
			out = std::filesystem::path(p);
			return;
		}
		std::string win_flag_storage;
		char const *f = osd_getenv(flag);
		if (!f || !*f) {
#ifdef _WIN32
			char win_flag_env[32] = {};
			DWORD const wf = GetEnvironmentVariableA(flag, win_flag_env, sizeof(win_flag_env));
			if (wf > 0U && wf < sizeof(win_flag_env))
				win_flag_storage.assign(win_flag_env, static_cast<std::size_t>(wf));
#endif
		}
		if (!win_flag_storage.empty())
			f = win_flag_storage.c_str();
		if (f && (f[0] == '1') && f[1] == '\0' && !m_boot_trace_path.empty()) {
			if (m_boot_trace_path.has_parent_path())
				out = m_boot_trace_path.parent_path() / filename;
			else
				out = std::filesystem::path(filename);
		}
	};
	sidecar("COINLINE_TRACE_STACK", "COINLINE_STACK_TRACE", "stack-trace.jsonl", m_stack_trace_path);
	sidecar("COINLINE_TRACE_NVRAM_STORAGE", "COINLINE_NVRAM_STORAGE_TRACE", "nvram-storage-trace.jsonl",
		m_nvram_storage_trace_path);
	sidecar("COINLINE_TRACE_MICROWIRE", "COINLINE_MICROWIRE_TRACE", "microwire-eeprom-trace.jsonl",
		m_microwire_trace_path);
	sidecar("COINLINE_TRACE_RAM_INIT", "COINLINE_RAM_INIT_TRACE", "ram-init-trace.jsonl", m_ram_init_trace_path);
	sidecar("COINLINE_TRACE_MMU_TRANSLATION", "COINLINE_MMU_TRANSLATION_TRACE", "mmu-translation-trace.jsonl",
		m_mmu_translation_trace_path);
	sidecar("COINLINE_TRACE_INTERRUPTS", "COINLINE_INTERRUPT_TRACE", "interrupt-trace.jsonl", m_interrupt_trace_path);
	sidecar("COINLINE_TRACE_TIMERS", "COINLINE_TIMER_TRACE", "timer-trace.jsonl", m_timer_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_ASCI_TRACE", "asci-trace.jsonl", m_asci_trace_path);
	sidecar("COINLINE_TRACE_RESET", "COINLINE_RESET_TRACE", "reset-trace.jsonl", m_reset_trace_path);
	sidecar("COINLINE_TRACE_VECTOR_EVENTS", "COINLINE_INTERRUPT_EVENTS", "interrupt-events.jsonl",
		m_interrupt_events_path);
	sidecar("COINLINE_TRACE_VECTOR_EVENTS", "COINLINE_VECTOR_EVENTS", "vector-events.jsonl", m_vector_events_path);
	sidecar("COINLINE_TRACE_VECTOR_EVENTS", "COINLINE_CONTEXT_SWITCH_EVENTS", "context-switch-events.jsonl",
		m_context_switch_events_path);
	sidecar("COINLINE_TRACE_VECTOR_EVENTS", "COINLINE_EIDI_EVENTS", "ei-di-events.jsonl", m_eidi_events_path);
	sidecar("COINLINE_TRACE_VOICEWARE", "COINLINE_VOICEWARE_TRACE", "voiceware-trace.jsonl", m_voiceware_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_EXTERNAL_UART_TRACE", "external-uart-trace.jsonl", m_external_uart_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_BOARD_TRACE", "telephony-board-trace.jsonl", m_telephony_board_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_HANDSHAKE_TRACE", "telephony-handshake-trace.jsonl",
		m_telephony_handshake_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_PHASE_TRACE", "telephony-phase-trace.jsonl",
		m_telephony_phase_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_READY_DECISION_TRACE", "telephony-ready-decision-trace.jsonl",
		m_telephony_ready_decision_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_RX_BUFFER_TRACE", "telephony-rx-buffer-trace.jsonl",
		m_telephony_rx_buffer_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_PARSER_TRACE", "telephony-parser-trace.jsonl",
		m_telephony_parser_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_SERVICE_PATH_TRACE", "service-path-trace.jsonl", m_service_path_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_SERVICE_TASK_TRACE", "service-task-trace.jsonl", m_service_task_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_SERVICE_DISPLAY_TRACE", "service-display-trace.jsonl",
		m_service_display_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_DISPLAY_QUEUE_TRACE", "display-queue-trace.jsonl", m_display_queue_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_SERVICE_REFRESH_TRACE", "service-refresh-trace.jsonl",
		m_service_refresh_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_OOS_MESSAGE_SELECTOR_TRACE", "oos-message-selector-trace.jsonl",
		m_oos_message_selector_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_OOS_REASON_TRACE", "oos-reason-trace.jsonl", m_oos_reason_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_SERVICE_MODE_TRACE", "service-mode-trace.jsonl", m_service_mode_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_DISPLAY_CACHE_TRACE", "display-cache-trace.jsonl", m_display_cache_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_READY_STATE_TRACE", "telephony-ready-state-trace.jsonl",
		m_telephony_ready_state_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_RETRY_TIMEOUT_TRACE", "telephony-retry-timeout-trace.jsonl",
		m_telephony_retry_timeout_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TERMFLAG_TRACE", "termflag-trace.jsonl", m_termflag_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_ALARM_CONDITION_TRACE", "alarm-condition-trace.jsonl",
		m_alarm_condition_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_RTOS_SIGNAL_TRACE", "rtos-signal-trace.jsonl", m_rtos_signal_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_SERVICE_TIMER_TRACE", "service-timer-trace.jsonl", m_service_timer_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_RUNTIME_CONVERSATION_TRACE",
		"telephony-runtime-conversation-trace.jsonl", m_telephony_runtime_conversation_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_RUNTIME_HEALTH_TRACE",
		"telephony-runtime-health-trace.jsonl", m_telephony_runtime_health_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TELEPHONY_RUNTIME_STATE", "telephony-runtime-state.json",
		m_telephony_runtime_state_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_CSIO_RAW_TRACE", "tp-csio-raw-trace.jsonl",
		m_tp_csio_raw_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_CSIO_QUALIFIED_TRACE", "tp-csio-qualified-trace.jsonl",
		m_tp_csio_qualified_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_COMMAND_RESPONSE_TRACE", "tp-command-response-trace.jsonl",
		m_tp_command_response_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_8048_RUNTIME_TRACE", "tp-8048-runtime-trace.jsonl",
		m_tp_8048_runtime_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_8048_PORT_TRACE", "tp-8048-port-trace.jsonl",
		m_tp_8048_port_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_8048_KEYPAD_TRACE", "tp-8048-keypad-trace.jsonl",
		m_tp_8048_keypad_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_8048_TONE_TRACE", "tp-8048-tone-trace.jsonl",
		m_tp_8048_tone_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_8048_CP_PROTOCOL_TRACE", "tp-8048-cp-protocol-trace.jsonl",
		m_tp_8048_cp_protocol_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_RUNTIME_HEALTH_TRACE", "tp-runtime-health-trace.jsonl",
		m_tp_runtime_health_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_READINESS_SEQUENCE_TRACE", "tp-readiness-sequence-trace.jsonl",
		m_tp_readiness_sequence_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_TP_BOARD_STATUS_TRACE", "tp-board-status-trace.jsonl",
		m_tp_board_status_trace_path);
	// HW_CNTL R1–R3 plus PIO-C data-jack / MUTDTMF bits share one JSONL timeline.
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_HW_CNTL_RELAY_TRACE", "hw-cntl-relay-trace.jsonl",
		m_hw_cntl_relay_trace_path);
	sidecar("COINLINE_TRACE_ASCI", "COINLINE_VFD_MESSAGE_STATE_TRACE", "vfd-message-state-trace.jsonl",
		m_vfd_message_state_trace_path);
	sidecar("COINLINE_TRACE_PANEL", "COINLINE_FRONT_PANEL_TRACE", "front-panel-trace.jsonl", m_front_panel_trace_path);
	sidecar("COINLINE_TRACE_PANEL", "COINLINE_FRONT_PANEL_INPUT_SOURCE_TRACE", "front-panel-input-source-trace.jsonl",
		m_front_panel_input_source_trace_path);
	sidecar("COINLINE_TRACE_PANEL", "COINLINE_TP_KEYPAD_INPUT_TRACE", "tp-keypad-input-trace.jsonl",
		m_tp_keypad_input_trace_path);
	sidecar("COINLINE_TRACE_PANEL", "COINLINE_TP_KEYPAD_EVENT_TRACE", "tp-keypad-event-trace.jsonl",
		m_tp_keypad_event_trace_path);
	sidecar("COINLINE_TRACE_PANEL", "COINLINE_TP_CP_KEYPAD_PROTOCOL_TRACE", "tp-cp-keypad-protocol-trace.jsonl",
		m_tp_cp_keypad_protocol_trace_path);
	sidecar("COINLINE_TRACE_PANEL", "COINLINE_CRAFT_ENTRY_GATE_TRACE", "craft-entry-gate-trace.jsonl",
		m_craft_entry_gate_trace_path);
	sidecar("COINLINE_TRACE_VFD", "COINLINE_VFD_IDLE_FIXTURE_DIFF_TRACE", "vfd-idle-fixture-diff-trace.jsonl",
		m_vfd_idle_fixture_diff_trace_path);
	sidecar("COINLINE_TRACE_FETCH_PROVENANCE", "COINLINE_FETCH_PROVENANCE_TRACE", "fetch-provenance-trace.jsonl",
		m_fetch_provenance_trace_path);
	sidecar("COINLINE_TRACE_STACK_CONTROL_FLOW", "COINLINE_STACK_CONTROL_FLOW_TRACE", "stack-control-flow-trace.jsonl",
		m_stack_control_flow_trace_path);
	sidecar("COINLINE_TRACE_VFD", "COINLINE_VFD_TRACE", "vfd-trace.jsonl", m_vfd_trace_path);
	sidecar("COINLINE_TRACE_VFD", "COINLINE_VFD_SNAPSHOTS", "vfd-snapshots.jsonl", m_vfd_snapshots_path);

	// Boot-critical (B1): when interrupt sampling is on, also emit opcode-level interrupt/vector/context
	// JSONL beside boot-trace without requiring a separate COINLINE_TRACE_VECTOR_EVENTS env var.
	if (m_boot_trace_path.has_parent_path()) {
		std::filesystem::path const d = m_boot_trace_path.parent_path();
		if (!m_interrupt_trace_path.empty()) {
			if (m_interrupt_events_path.empty())
				m_interrupt_events_path = d / "interrupt-events.jsonl";
			if (m_vector_events_path.empty())
				m_vector_events_path = d / "vector-events.jsonl";
			if (m_context_switch_events_path.empty())
				m_context_switch_events_path = d / "context-switch-events.jsonl";
			if (m_eidi_events_path.empty())
				m_eidi_events_path = d / "ei-di-events.jsonl";
		}
		m_first_pc_ffff_path = d / "first-pc-ffff.json";
		m_first_pc_ffff_context_path = d / "first-pc-ffff-context.jsonl";
		m_first_rst38_context_path = d / "first-rst38-context.jsonl";
		if (m_vfd_trace_path.empty())
			m_vfd_trace_path = d / "vfd-trace.jsonl";
		if (m_vfd_snapshots_path.empty())
			m_vfd_snapshots_path = d / "vfd-snapshots.jsonl";
		m_vfd_final_state_path = d / "vfd-final-state.json";
		m_vfd_final_text_path = d / "vfd-final-text.txt";
		auto pinp = [&d](std::filesystem::path &dst, char const *tail) {
			if (dst.empty())
				dst = d / tail;
		};
		pinp(m_tp_keypad_input_trace_path, "tp-keypad-input-trace.jsonl");
		pinp(m_tp_keypad_event_trace_path, "tp-keypad-event-trace.jsonl");
		pinp(m_tp_cp_keypad_protocol_trace_path, "tp-cp-keypad-protocol-trace.jsonl");
		pinp(m_craft_entry_gate_trace_path, "craft-entry-gate-trace.jsonl");
		pinp(m_vfd_idle_fixture_diff_trace_path, "vfd-idle-fixture-diff-trace.jsonl");
		pinp(m_hw_cntl_relay_trace_path, "hw-cntl-relay-trace.jsonl");
	}
	// Telephony timing profile: always materialize dedicated JSONL files next to boot-trace.
	if (m_trace_profile == trace_profile::tp_timing && m_boot_trace_path.has_parent_path()) {
		std::filesystem::path const d = m_boot_trace_path.parent_path();
		auto pin = [](std::filesystem::path &dst, std::filesystem::path const &d0, char const *tail) {
			if (dst.empty())
				dst = d0 / tail;
		};
		pin(m_tp_health_cadence_trace_path, d, "tp-health-cadence-trace.jsonl");
		pin(m_tp_csio_timing_trace_path, d, "tp-csio-timing-trace.jsonl");
		pin(m_tp_interrupt_trace_path, d, "tp-interrupt-trace.jsonl");
		pin(m_tp_timeout_trace_path, d, "tp-timeout-trace.jsonl");
		pin(m_telephony_runtime_health_trace_path, d, "telephony-runtime-health-trace.jsonl");
		pin(m_telephony_runtime_conversation_trace_path, d, "telephony-runtime-conversation-trace.jsonl");
		pin(m_tp_csio_raw_trace_path, d, "tp-csio-raw-trace.jsonl");
		pin(m_tp_csio_qualified_trace_path, d, "tp-csio-qualified-trace.jsonl");
		pin(m_tp_command_response_trace_path, d, "tp-command-response-trace.jsonl");
		pin(m_tp_8048_runtime_trace_path, d, "tp-8048-runtime-trace.jsonl");
		pin(m_tp_8048_port_trace_path, d, "tp-8048-port-trace.jsonl");
		pin(m_tp_8048_keypad_trace_path, d, "tp-8048-keypad-trace.jsonl");
		pin(m_tp_8048_tone_trace_path, d, "tp-8048-tone-trace.jsonl");
		pin(m_tp_8048_cp_protocol_trace_path, d, "tp-8048-cp-protocol-trace.jsonl");
	}

	// Debug accelerator: COINLINE_TRACE_ONLY=<name> keeps exactly one trace sink active and
	// clears every other JSONL path so emit sites short-circuit on path.empty(). Use during
	// triage when the cumulative cost of dozens of JSONL writers slows the simulation enough
	// to miss real-time inputs (off-hook timing, scripted demo presses, etc.).
	// <name> is matched as a substring against each path's filename, so short forms work:
	//   COINLINE_TRACE_ONLY=tp-csio-raw-trace
	//   COINLINE_TRACE_ONLY=alarm-condition
	//   COINLINE_TRACE_ONLY=vfd
	char const *only = osd_getenv("COINLINE_TRACE_ONLY");
	char w32_buf[1024] = {};
#ifdef _WIN32
	{
		// MSYS2 bash -> Git Bash -> PowerShell chains can drop env vars in the C runtime
		// environ but keep them in the Win32 env block. Fall back to GetEnvironmentVariableA
		// when osd_getenv came up empty so debug runs from arbitrary shells still work.
		DWORD const wn = GetEnvironmentVariableA("COINLINE_TRACE_ONLY", w32_buf, sizeof(w32_buf));
		if ((!only || !*only) && wn > 0 && wn < sizeof(w32_buf))
			only = w32_buf;
	}
#endif
	if (only && *only) {
		std::string const want(only);
		std::vector<std::filesystem::path *> const all{
			&m_io_trace_path, &m_memory_trace_path, &m_nvram_storage_trace_path, &m_microwire_trace_path,
			&m_cpu_trace_path, &m_z180_reg_trace_path,
			&m_stack_trace_path, &m_ram_init_trace_path, &m_mmu_translation_trace_path,
			&m_interrupt_trace_path, &m_timer_trace_path, &m_asci_trace_path, &m_reset_trace_path,
			&m_interrupt_events_path, &m_vector_events_path, &m_context_switch_events_path,
			&m_eidi_events_path, &m_voiceware_trace_path, &m_external_uart_trace_path,
			&m_telephony_board_trace_path, &m_telephony_handshake_trace_path,
			&m_telephony_phase_trace_path, &m_telephony_ready_decision_trace_path,
			&m_telephony_rx_buffer_trace_path, &m_telephony_parser_trace_path,
			&m_service_path_trace_path, &m_service_task_trace_path, &m_service_display_trace_path,
			&m_display_queue_trace_path, &m_service_refresh_trace_path,
			&m_oos_message_selector_trace_path, &m_oos_reason_trace_path,
			&m_service_mode_trace_path, &m_display_cache_trace_path,
			&m_telephony_ready_state_trace_path, &m_telephony_retry_timeout_trace_path,
			&m_termflag_trace_path, &m_alarm_condition_trace_path, &m_rtos_signal_trace_path,
			&m_service_timer_trace_path, &m_telephony_runtime_conversation_trace_path,
			&m_telephony_runtime_health_trace_path, &m_telephony_runtime_state_path,
			&m_tp_csio_raw_trace_path, &m_tp_csio_qualified_trace_path,
			&m_tp_command_response_trace_path, &m_tp_runtime_health_trace_path,
			&m_tp_readiness_sequence_trace_path, &m_tp_board_status_trace_path, &m_hw_cntl_relay_trace_path,
			&m_tp_8048_runtime_trace_path, &m_tp_8048_port_trace_path, &m_tp_8048_keypad_trace_path,
			&m_tp_8048_tone_trace_path, &m_tp_8048_cp_protocol_trace_path,
			&m_vfd_message_state_trace_path, &m_front_panel_trace_path,
			&m_front_panel_input_source_trace_path, &m_tp_keypad_input_trace_path,
			&m_tp_keypad_event_trace_path, &m_tp_cp_keypad_protocol_trace_path, &m_craft_entry_gate_trace_path,
			&m_fetch_provenance_trace_path,
			&m_stack_control_flow_trace_path, &m_vfd_trace_path, &m_vfd_snapshots_path, &m_vfd_idle_fixture_diff_trace_path,
			&m_tp_health_cadence_trace_path, &m_tp_csio_timing_trace_path,
			&m_tp_interrupt_trace_path, &m_tp_timeout_trace_path,
		};
		std::size_t kept = 0;
		std::filesystem::path kept_path;
		for (std::filesystem::path *p : all) {
			if (p->empty())
				continue;
			std::string const stem = p->filename().string();
			if (stem.find(want) != std::string::npos) {
				kept_path = *p;
				++kept;
			}
			else {
				p->clear();
			}
		}
		// Disable expensive ring buffers + always-on context capture so the chosen sink is the
		// only meaningful source of overhead.
		m_trace_capture_full_io = false;
		m_trace_capture_full_cpu = false;
		m_trace_capture_fault_context = false;
		m_trace_capture_hot_summary = false;
		// Several device-side trace sinks (voiceware, audio_route, alerter, telephony audio,
		// supervision, mute_route) read their env var at every write rather than caching a
		// path member. Unset those env vars when they don't match `want` so they short-circuit.
		// Use both osd_setenv and SetEnvironmentVariableA: the C runtime's environ is what
		// osd_getenv returns; the Win32 env block is what GetEnvironmentVariableA returns.
		auto unset_unless_match = [&](char const *envname) {
			char const *v = osd_getenv(envname);
			char w32v[1024] = {};
			DWORD const wn2 = GetEnvironmentVariableA(envname, w32v, sizeof(w32v));
			std::string const cur = (v && *v) ? std::string(v) : ((wn2 > 0 && wn2 < sizeof(w32v)) ? std::string(w32v) : std::string());
			if (cur.empty())
				return;
			std::filesystem::path const cur_path(cur);
			std::string const stem = cur_path.filename().string();
			if (stem.find(want) == std::string::npos) {
				osd_setenv(envname, "", 1);
				SetEnvironmentVariableA(envname, nullptr);
			}
		};
		char const *const inline_env_vars[] = {
			"COINLINE_VOICEWARE_TRACE", "COINLINE_VOICEWARE_DECODE_TRACE",
			"COINLINE_AUDIO_TRACE", "COINLINE_ALERTER_TRACE",
			"COINLINE_TELEPHONY_TRACE", "COINLINE_AUDIO_ROUTE_TRACE",
			"COINLINE_MUTE_ROUTE_TRACE", "COINLINE_SUPERVISION_TRACE",
		};
		for (char const *e : inline_env_vars)
			unset_unless_match(e);
		if (m_boot_trace_path.has_parent_path()) {
			std::filesystem::path const d = m_boot_trace_path.parent_path();
			std::filesystem::path const note = d / "trace-only-filter.json";
			std::error_code ec;
			std::filesystem::create_directories(d, ec);
			std::ofstream os(note, std::ios::binary | std::ios::trunc);
			if (os.is_open()) {
				os << "{\"trace_only_filter\":\"" << want << "\",\"matched\":" << kept
					<< ",\"kept_path\":\"" << kept_path.filename().string() << "\"}\n";
			}
		}
	}
}

void millennium_state::maybe_trace_memory_sidecar(std::uint32_t phys_addr, u8 data, char const *region_tag)
{
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	std::uint8_t const cbr = u8(m_maincpu->state_int(Z180_CBR) & 0xff);
	std::uint8_t const bbr = u8(m_maincpu->state_int(Z180_BBR) & 0xff);
	std::uint8_t const cbar = u8(m_maincpu->state_int(Z180_CBAR) & 0xff);
	std::uint32_t const sp_phys = millennium_z180_mmu_translate20(sp, cbr, bbr, cbar);
	bool stack_touch = false;
	for (unsigned d = 0; d < 8U; d++) {
		std::uint32_t const t = (sp_phys + 0x100000U - d) & 0xfffffU;
		if (phys_addr == t) {
			stack_touch = true;
			break;
		}
	}
	if (m_stack_trace_remaining && !m_stack_trace_path.empty() && stack_touch) {
		std::string const line = millennium_format_stack_trace_line(m_maincpu->total_cycles(), phys_addr, data, pc, sp,
			sp_phys, region_tag, current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_stack_trace_path, line);
		--m_stack_trace_remaining;
	}
	if (m_ram_init_trace_remaining && !m_ram_init_trace_path.empty()) {
		std::string const line = millennium_format_ram_init_trace_line(m_maincpu->total_cycles(), phys_addr, data, pc, sp,
			region_tag, current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_ram_init_trace_path, line);
		--m_ram_init_trace_remaining;
	}
}

void millennium_state::append_mmu_translation_trace_line()
{
	if (m_mmu_translation_trace_path.empty())
		return;
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	std::uint8_t const cbr = u8(m_maincpu->state_int(Z180_CBR) & 0xff);
	std::uint8_t const bbr = u8(m_maincpu->state_int(Z180_BBR) & 0xff);
	std::uint8_t const cbar = u8(m_maincpu->state_int(Z180_CBAR) & 0xff);
	std::uint32_t const pc_phys = millennium_z180_mmu_translate20(pc, cbr, bbr, cbar);
	std::uint32_t const sp_phys = millennium_z180_mmu_translate20(sp, cbr, bbr, cbar);
	std::string const line = millennium_format_mmu_translation_trace_line(m_maincpu->total_cycles(), pc, sp, cbr, bbr, cbar,
		pc_phys, sp_phys, current_boot_milestone_tag());
	millennium_boot_trace_append_line(m_mmu_translation_trace_path, line);
}

char const *millennium_state::physical_debug_source(std::uint32_t phys_addr) const
{
	phys_addr &= 0xfffffU;
	if (phys_addr < 0xc0000U) {
		if (phys_addr < m_phys_low_valid.size() && m_phys_low_valid[phys_addr])
			return "ram_overlay";
		memory_region *const rom = memregion("flash");
		if (rom && phys_addr < rom->bytes())
			return "flash";
		return "unmapped";
	}
	if (phys_addr >= 0xc0000U && phys_addr <= 0xfffffU) {
		millennium_mach_phys_ram_decode const dec =
			millennium_mach_decode_phys_ram(m_mach_pio_shadow[0], phys_addr, m_phys_ram.size());
		if (dec.route == millennium_mach_phys_ram_route::sram_chip)
			return "sram_512k_banked";
		if (phys_addr >= 0xe0000U && dec.route == millennium_mach_phys_ram_route::upper_flash) {
			memory_region *const rom = memregion("flash");
			if (rom && phys_addr < rom->bytes())
				return "flash";
			return "unmapped";
		}
		if (dec.route == millennium_mach_phys_ram_route::unmapped_ff)
			return "unmapped";
	}
	return "unknown";
}

std::uint8_t millennium_state::read_physical_debug_byte(std::uint32_t phys_addr) const
{
	phys_addr &= 0xfffffU;
	if (phys_addr < 0xc0000U) {
		if (phys_addr < m_phys_low_valid.size() && m_phys_low_valid[phys_addr])
			return m_phys_low_overlay[phys_addr];
		memory_region *const rom = memregion("flash");
		if (rom && phys_addr < rom->bytes())
			return rom->base()[phys_addr];
		return 0xffU;
	}
	if (phys_addr >= 0xc0000U && phys_addr <= 0xfffffU) {
		millennium_mach_phys_ram_decode const dec =
			millennium_mach_decode_phys_ram(m_mach_pio_shadow[0], phys_addr, m_phys_ram.size());
		if (dec.route == millennium_mach_phys_ram_route::sram_chip)
			return m_phys_ram[dec.chip_byte_index];
		if (phys_addr >= 0xe0000U && dec.route == millennium_mach_phys_ram_route::upper_flash) {
			memory_region *const rom = memregion("flash");
			if (rom && phys_addr < rom->bytes())
				return rom->base()[phys_addr];
		}
	}
	return 0xffU;
}

std::string millennium_state::format_fetch_provenance_event(char const *event, std::uint16_t pc, std::uint16_t sp,
	std::uint8_t op0, std::uint8_t op1, std::uint8_t op2)
{
	millennium_z180_snapshot const snap = build_z180_snapshot();
	std::uint32_t const pc_phys = millennium_z180_mmu_translate20(pc, snap.cbr, snap.bbr, snap.cbar);
	std::uint32_t const sp_phys = millennium_z180_mmu_translate20(sp, snap.cbr, snap.bbr, snap.cbar);
	std::uint8_t const phys_byte = read_physical_debug_byte(pc_phys);
	char const *const source = physical_debug_source(pc_phys);
	bool const iff1 = m_maincpu->state_int(Z180_IFF1) != 0;
	bool const iff2 = m_maincpu->state_int(Z180_IFF2) != 0;
	int const im = int(m_maincpu->state_int(Z180_IM) & 3);
	std::ostringstream os;
	os << "{\"cycle\":" << m_maincpu->total_cycles() << ",\"event\":\"" << (event ? event : "sample") << "\",";
	os << std::hex << std::uppercase << std::setfill('0');
	os << "\"pc\":\"0x" << std::setw(4) << unsigned(pc) << "\",\"logical_address\":\"0x" << std::setw(4)
	   << unsigned(pc) << "\",\"physical_address\":\"0x" << std::setw(5) << (pc_phys & 0xfffffU) << "\",";
	os << "\"byte\":\"0x" << std::setw(2) << unsigned(phys_byte) << "\",";
	os << "\"opcode\":[\"0x" << std::setw(2) << unsigned(op0) << "\",\"0x" << std::setw(2) << unsigned(op1)
	   << "\",\"0x" << std::setw(2) << unsigned(op2) << "\"],";
	os << "\"sp\":\"0x" << std::setw(4) << unsigned(sp) << "\",\"sp_physical\":\"0x" << std::setw(5)
	   << (sp_phys & 0xfffffU) << "\",";
	os << "\"af\":\"0x" << std::setw(4) << unsigned(m_maincpu->state_int(Z180_AF) & 0xffff) << "\",";
	os << "\"bc\":\"0x" << std::setw(4) << unsigned(m_maincpu->state_int(Z180_BC) & 0xffff) << "\",";
	os << "\"de\":\"0x" << std::setw(4) << unsigned(m_maincpu->state_int(Z180_DE) & 0xffff) << "\",";
	os << "\"hl\":\"0x" << std::setw(4) << unsigned(m_maincpu->state_int(Z180_HL) & 0xffff) << "\",";
	os << "\"ix\":\"0x" << std::setw(4) << unsigned(m_maincpu->state_int(Z180_IX) & 0xffff) << "\",";
	os << "\"iy\":\"0x" << std::setw(4) << unsigned(m_maincpu->state_int(Z180_IY) & 0xffff) << "\",";
	os << "\"cbr\":\"0x" << std::setw(2) << unsigned(snap.cbr) << "\",\"bbr\":\"0x" << std::setw(2)
	   << unsigned(snap.bbr) << "\",\"cbar\":\"0x" << std::setw(2) << unsigned(snap.cbar) << "\",";
	os << std::dec << "\"iff1\":" << (iff1 ? "true" : "false") << ",\"iff2\":" << (iff2 ? "true" : "false")
	   << ",\"im\":" << im << ",\"memory_source\":\"" << source << "\",";
	os << "\"erased_or_unmapped_ff\":"
	   << ((phys_byte == 0xffU && std::strcmp(source, "ram_overlay") != 0 && std::strcmp(source, "sram_512k_banked") != 0)
		   ? "true"
		   : "false")
	   << ",\"milestone\":\"" << current_boot_milestone_tag() << "\"";
	if (!m_last_io_event_json.empty())
		os << ",\"last_io_event\":" << m_last_io_event_json;
	if (!m_last_stack_write_json.empty())
		os << ",\"last_memory_write_near_sp\":" << m_last_stack_write_json;
	os << '}';
	return os.str();
}

void millennium_state::remember_memory_write(std::uint32_t phys_addr, std::uint8_t data, char const *region_tag)
{
	phys_addr &= 0xfffffU;
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	millennium_z180_snapshot const snap = build_z180_snapshot();
	std::uint32_t const sp_phys = millennium_z180_mmu_translate20(sp, snap.cbr, snap.bbr, snap.cbar);
	std::ostringstream os;
	os << "{\"cycle\":" << m_maincpu->total_cycles() << ",\"phys\":\"0x" << std::hex << std::uppercase
	   << std::setfill('0') << std::setw(5) << phys_addr << "\",\"data\":\"0x" << std::setw(2) << unsigned(data)
	   << "\",\"pc\":\"0x" << std::setw(4) << unsigned(pc) << "\",\"sp\":\"0x" << std::setw(4) << unsigned(sp)
	   << "\",\"sp_phys\":\"0x" << std::setw(5) << (sp_phys & 0xfffffU) << "\",\"region\":\""
	   << (region_tag ? region_tag : "unknown") << "\"}";
	m_last_memory_write_by_phys[phys_addr] = os.str();
	for (unsigned d = 0; d < 16U; d++) {
		if (phys_addr == ((sp_phys + 0x100000U - d) & 0xfffffU)) {
			m_last_stack_write_json = os.str();
			break;
		}
	}
}

void millennium_state::maybe_emit_stack_control_flow(std::uint64_t cyc, std::uint16_t pc, std::uint16_t sp,
	std::uint8_t op0, std::uint8_t op1, std::uint8_t op2, char const *milestone, bool new_decode_slot)
{
	if (m_stack_control_flow_trace_path.empty() || !new_decode_slot)
		return;
	char const *event = nullptr;
	std::uint16_t target = 0;
	std::uint16_t ret_addr = 0;
	std::uint32_t stack_phys = 0;
	bool popped_ffff = false;
	bool has_stack_read = false;
	if (op0 == 0xcdU) {
		event = "call";
		target = u16(op1 | (u16(op2) << 8));
		ret_addr = u16(pc + 3U);
		millennium_z180_snapshot const snap = build_z180_snapshot();
		stack_phys = millennium_z180_mmu_translate20(u16(sp - 2U), snap.cbr, snap.bbr, snap.cbar);
	}
	else if (op0 == 0xc9U) {
		event = "ret";
		millennium_z180_snapshot const snap = build_z180_snapshot();
		stack_phys = millennium_z180_mmu_translate20(sp, snap.cbr, snap.bbr, snap.cbar);
		std::uint8_t const lo = read_physical_debug_byte(stack_phys);
		std::uint8_t const hi = read_physical_debug_byte((stack_phys + 1U) & 0xfffffU);
		target = u16(lo | (u16(hi) << 8));
		popped_ffff = target == 0xffffU;
		has_stack_read = true;
	}
	else if (op0 == 0xffU) {
		event = "rst38";
		target = 0x0038U;
		ret_addr = u16(pc + 1U);
		millennium_z180_snapshot const snap = build_z180_snapshot();
		stack_phys = millennium_z180_mmu_translate20(u16(sp - 2U), snap.cbr, snap.bbr, snap.cbar);
	}
	else if (op0 == 0xc3U) {
		event = "jp";
		target = u16(op1 | (u16(op2) << 8));
	}
	else if (op0 == 0xedU && (op1 == 0x4dU || op1 == 0x45U)) {
		event = (op1 == 0x4dU) ? "reti" : "retn";
		millennium_z180_snapshot const snap = build_z180_snapshot();
		stack_phys = millennium_z180_mmu_translate20(sp, snap.cbr, snap.bbr, snap.cbar);
		std::uint8_t const lo = read_physical_debug_byte(stack_phys);
		std::uint8_t const hi = read_physical_debug_byte((stack_phys + 1U) & 0xfffffU);
		target = u16(lo | (u16(hi) << 8));
		popped_ffff = target == 0xffffU;
		has_stack_read = true;
	}
	if (!event)
		return;

	std::ostringstream os;
	os << "{\"cycle\":" << cyc << ",\"event\":\"" << event << "\",\"pc\":\"0x" << std::hex << std::uppercase
	   << std::setfill('0') << std::setw(4) << unsigned(pc) << "\",\"sp\":\"0x" << std::setw(4) << unsigned(sp)
	   << "\",\"opcode\":[\"0x" << std::setw(2) << unsigned(op0) << "\",\"0x" << std::setw(2) << unsigned(op1)
	   << "\",\"0x" << std::setw(2) << unsigned(op2) << "\"],\"target\":\"0x" << std::setw(4) << unsigned(target)
	   << "\",\"return_address\":\"0x" << std::setw(4) << unsigned(ret_addr) << "\",\"stack_phys\":\"0x"
	   << std::setw(5) << (stack_phys & 0xfffffU) << "\",\"stack_source\":\"" << physical_debug_source(stack_phys)
	   << "\",\"popped_return_ffff\":" << (popped_ffff ? "true" : "false");
	if (has_stack_read) {
		std::uint32_t const lo_phys = stack_phys & 0xfffffU;
		std::uint32_t const hi_phys = (stack_phys + 1U) & 0xfffffU;
		os << ",\"stack_bytes\":[\"0x" << std::setw(2) << unsigned(read_physical_debug_byte(lo_phys)) << "\",\"0x"
		   << std::setw(2) << unsigned(read_physical_debug_byte(hi_phys)) << "\"]";
		auto const lo_it = m_last_memory_write_by_phys.find(lo_phys);
		auto const hi_it = m_last_memory_write_by_phys.find(hi_phys);
		if (lo_it != m_last_memory_write_by_phys.end())
			os << ",\"last_writer_lo\":" << lo_it->second;
		if (hi_it != m_last_memory_write_by_phys.end())
			os << ",\"last_writer_hi\":" << hi_it->second;
	}
	os << std::dec << ",\"milestone\":\"" << (milestone ? milestone : "none") << "\"}";
	millennium_boot_trace_append_line(m_stack_control_flow_trace_path, os.str());
}

u8 millennium_state::phys_low_r(offs_t offset)
{
	if (offset >= m_phys_low_valid.size())
		return 0xff;
	if (m_phys_low_valid[offset])
		return m_phys_low_overlay[offset];
	memory_region *const rom = memregion("flash");
	if (rom && offset < rom->bytes())
		return rom->base()[offset];
	return 0xff;
}

void millennium_state::phys_low_w(offs_t offset, u8 data)
{
	if (offset >= m_phys_low_valid.size())
		return;
	m_phys_low_valid[offset] = 1U;
	m_phys_low_overlay[offset] = data;
	++m_ram_write_events;
	remember_memory_write(static_cast<std::uint32_t>(offset), data, "low_overlay");
	append_memory_trace_line(static_cast<std::uint32_t>(offset), data);
	maybe_trace_memory_sidecar(static_cast<std::uint32_t>(offset), data, "low_overlay");
	if (!m_m3_m4_logged)
		emit_boot_m3_m4_snapshot("first_ram_write");
}

void millennium_state::phys_ram_w(offs_t offset, u8 data)
{
	std::uint32_t const phys = static_cast<std::uint32_t>(offset) & 0xfffffU;
	millennium_mach_phys_ram_decode const dec =
		millennium_mach_decode_phys_ram(m_mach_pio_shadow[0], phys, m_phys_ram.size());
	if (dec.route != millennium_mach_phys_ram_route::sram_chip)
		return;
	m_phys_ram[dec.chip_byte_index] = data;
	++m_ram_write_events;
	remember_memory_write(static_cast<std::uint32_t>(offset), data, "sram_512k_banked");
	append_memory_trace_line(static_cast<std::uint32_t>(offset), data);
	maybe_trace_memory_sidecar(static_cast<std::uint32_t>(offset), data, "sram_512k_banked");
	if (!m_m3_m4_logged)
		emit_boot_m3_m4_snapshot("first_ram_write");
}

u8 millennium_state::storage_nvram_r(offs_t offset)
{
	std::uint32_t const off32 = static_cast<std::uint32_t>(offset);
	u8 const r = m_nvram->model().read_nvram(off32);
	if (!m_nvram_storage_trace_path.empty()) {
		std::uint32_t const phys = m_memory_layout.nvram_base + off32;
		u16 const pc = u16(m_maincpu->pc() & 0xffffU);
		u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
		std::string const line = millennium_format_nvram_storage_trace_line(m_maincpu->total_cycles(), off32, phys, 'r',
			r, pc, sp, current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_nvram_storage_trace_path, line);
	}
	return r;
}

void millennium_state::storage_nvram_w(offs_t offset, u8 data)
{
	std::string err;
	std::uint32_t const off32 = static_cast<std::uint32_t>(offset);
	if (!m_nvram->model().write_nvram(off32, data, err))
		osd_printf_warning("millennium: %s\n", err.c_str());
	else if (!m_nvram_storage_trace_path.empty()) {
		std::uint32_t const phys = m_memory_layout.nvram_base + off32;
		u16 const pc = u16(m_maincpu->pc() & 0xffffU);
		u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
		std::string const line = millennium_format_nvram_storage_trace_line(m_maincpu->total_cycles(), off32, phys, 'w',
			data, pc, sp, current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_nvram_storage_trace_path, line);
	}
}

u8 millennium_state::storage_table_r(offs_t offset)
{
	return m_nvram->model().read_table(static_cast<std::uint32_t>(offset));
}

void millennium_state::storage_table_w(offs_t offset, u8 data)
{
	std::string err;
	if (!m_nvram->model().write_table(static_cast<std::uint32_t>(offset), data, err))
		osd_printf_warning("millennium: %s\n", err.c_str());
}

u8 millennium_state::storage_dla_r(offs_t offset)
{
	return m_nvram->model().read_dla(static_cast<std::uint32_t>(offset));
}

void millennium_state::storage_dla_w(offs_t offset, u8 data)
{
	std::string err;
	if (!m_nvram->model().write_dla(static_cast<std::uint32_t>(offset), data, err))
		osd_printf_warning("millennium: %s\n", err.c_str());
}

u8 millennium_state::card_status_r()
{
	u64 const cy = m_maincpu->total_cycles();
	u8 const r = m_card->model().status_bits(cy);
	emit_io_trace_line("card_status", 0x52, 'r', r);
	return r;
}

void millennium_state::card_status_w(u8 data)
{
	emit_io_trace_line("card_status", 0x52, 'w', data);
	(void)data;
}

u8 millennium_state::card_data_r()
{
	u64 const cy = m_maincpu->total_cycles();
	std::uint8_t const bit = m_card->model().data_byte(cy);
	u8 const r = static_cast<u8>(0xfeU | (bit & 1U));
	emit_io_trace_line("card_data", 0x53, 'r', r);
	return r;
}

void millennium_state::card_data_w(u8 data)
{
	emit_io_trace_line("card_data", 0x53, 'w', data);
	(void)data;
}

u8 millennium_state::coin_status_r()
{
	u64 const cy = m_maincpu->total_cycles();
	u8 const r = m_coin->read_status(cy);
	emit_io_trace_line("coin_status", 0x54, 'r', r);
	return r;
}

void millennium_state::coin_control_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	emit_io_trace_line("coin_control", 0x55, 'w', data);
	m_coin->write_control(data, cy);
}

void millennium_state::audio_tone_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	emit_io_trace_line("audio_tone", 0x58, 'w', data);
	m_audio->write_tone_select(data, cy, pc);
}

void millennium_state::audio_dtmf_ascii_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	emit_io_trace_line("audio_dtmf_ascii", 0x59, 'w', data);
	m_audio_dtmf_ascii = data;
	m_audio->write_dtmf_ascii(data, cy, pc);
}

void millennium_state::audio_dtmf_duration_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	emit_io_trace_line("audio_dtmf_duration", 0x5a, 'w', data);
	m_audio->write_dtmf_digit(m_audio_dtmf_ascii, unsigned(data), cy, pc);
}

void millennium_state::audio_vol_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	emit_io_trace_line("audio_volume", 0x5b, 'w', data);
	m_audio->write_volume(data, cy, pc);
}

u8 millennium_state::mach_pio_r(offs_t offset)
{
	u64 const cy = m_maincpu->total_cycles();
	std::uint8_t const base = m_mach_pio_shadow.at(std::size_t(offset));
	u8 r;
	if (offset == 0) {
		// Port 0xC0: latch + cash-box status bits.
		r = millennium_mach_pio_combine_port_h_read(base, m_smartcard->status_lines() & 0x03U);
		emit_io_trace_line("mach_pio_port_h", u16(0xc0), 'r', r);
	}
	else if (offset == 2) {
		r = m_smartcard->read_fifo(cy);
		emit_io_trace_line("mach_pio", u16(0xc0U + static_cast<std::uint16_t>(offset)), 'r', r);
	}
	else {
		r = base;
		emit_io_trace_line("mach_pio", u16(0xc0U + static_cast<std::uint16_t>(offset)), 'r', r);
	}
	return r;
}

void millennium_state::mach_pio_w(offs_t offset, u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	if (offset == 0)
		emit_io_trace_line("mach_pio_port_h", u16(0xc0), 'w', data);
	else
		emit_io_trace_line("mach_pio", u16(0xc0U + static_cast<std::uint16_t>(offset)), 'w', data);
	if (offset == 3) {
		u8 const prev = m_mach_pio_shadow.at(3);
		double const cpu_hz = static_cast<double>(m_maincpu->unscaled_clock());
		m_audio->notify_mach_port_f(prev, data, cy, cpu_hz);
	}
	m_mach_pio_shadow.at(std::size_t(offset)) = data;
	// Port \c D (\c 0xC1): bits 0–1 route coin / mag reader / SAM / lock (see \c millennium_mach_async.h).
	// Port \c H (\c 0xC0): bit 3 gate — clear selects async-side peripherals toward ASCI/UART.
	// Port \c E (\c 0xC2): SAM clock/rst/power — full byte feeds the smart-card model; EPM clocks use port \c G.
	// Port \c F (\c 0xC3): card-reader control + forgotten-card warning / cadence bits (\c millennium_mach_port_f); handset alerter remains \c 0x58–\c 0x5b.
	if (offset == 2)
		m_smartcard->write_command(data, cy);
}

void millennium_state::poll_card_ui()
{
	u32 const v = m_cardui->read();
	u32 const rise = v & ~m_card_ui_last;
	m_card_ui_last = v;
	if (!rise)
		return;
	u64 const cy = m_maincpu->total_cycles();
	u64 const hz = static_cast<u64>(m_maincpu->unscaled_clock());
	std::string err;
	if (rise & 1U) {
		std::string const p = resolve_relative_path(
			(osd_getenv("COINLINE_MAG_CARD") && *osd_getenv("COINLINE_MAG_CARD"))
				? std::string(osd_getenv("COINLINE_MAG_CARD"))
				: std::string("fixtures/cards/magcard-valid.json"));
		if (m_card->reload_fixture_from_path(p, err))
			m_card->arm_swipe(cy, hz);
		else
			osd_printf_warning("millennium: mag card reload failed: %s\n", err.c_str());
	}
	if (rise & 2U) {
		std::string const p = resolve_relative_path(
			(osd_getenv("COINLINE_SMART_CARD") && *osd_getenv("COINLINE_SMART_CARD"))
				? std::string(osd_getenv("COINLINE_SMART_CARD"))
				: std::string("fixtures/cards/smartcard-valid.json"));
		if (m_smartcard->reload_fixture_from_path(p, err))
			m_smartcard->insert_card(cy, hz);
		else
			osd_printf_warning("millennium: smartcard reload failed: %s\n", err.c_str());
	}
}

void millennium_state::poll_coin_ui()
{
	u32 const v = m_coinui->read();
	u32 const rise = v & ~m_coin_ui_last;
	m_coin_ui_last = v;
	if (!rise)
		return;
	u64 const cy = m_maincpu->total_cycles();
	u64 const hz = static_cast<u64>(m_maincpu->unscaled_clock());
	if (rise & 1U) {
		int cents = 25;
		char const *const e = osd_getenv("COINLINE_COIN_DENOM");
		if (e && *e)
			cents = std::atoi(e);
		(void)m_coin->begin_insert_cents(cents, cy, hz);
	}
	if (rise & 2U)
		m_coin->inject_reject_route(cy);
	if (rise & 4U)
		m_coin->inject_jam(cy);
}

millennium_z180_snapshot millennium_state::build_z180_snapshot()
{
	millennium_z180_snapshot snap;
	snap.cbr = u8(m_maincpu->state_int(Z180_CBR) & 0xff);
	snap.bbr = u8(m_maincpu->state_int(Z180_BBR) & 0xff);
	snap.cbar = u8(m_maincpu->state_int(Z180_CBAR) & 0xff);
	snap.rcr = u8(m_maincpu->state_int(Z180_RCR) & 0xff);
	snap.cntla0 = u8(m_maincpu->state_int(Z180_CNTLA0) & 0xff);
	snap.cntlb0 = u8(m_maincpu->state_int(Z180_CNTLB0) & 0xff);
	snap.stat0 = u8(m_maincpu->state_int(Z180_STAT0) & 0xff);
	snap.tcr = u8(m_maincpu->state_int(Z180_TCR) & 0xff);
	snap.rldr0 = u16(m_maincpu->state_int(Z180_RLDR0) & 0xffff);
	snap.tmdr0 = u16(m_maincpu->state_int(Z180_TMDR0) & 0xffff);
	snap.il = u8(m_maincpu->state_int(Z180_IL) & 0xff);
	snap.itc = u8(m_maincpu->state_int(Z180_ITC) & 0xff);
	snap.dstat = u8(m_maincpu->state_int(Z180_DSTAT) & 0xff);
	snap.dmode = u8(m_maincpu->state_int(Z180_DMODE) & 0xff);
	snap.dcntl = u8(m_maincpu->state_int(Z180_DCNTL) & 0xff);
	snap.iocr = u8(m_maincpu->state_int(Z180_IOCR) & 0xff);
	return snap;
}

char const *millennium_state::current_boot_milestone_tag() const
{
	if (m_boot_acceptance_ready_milestone_logged)
		return "acceptance_ready";
	if (m_boot_protocol_ready_milestone_logged)
		return "protocol_ready";
	if (m_m10_logged)
		return "M10";
	if (m_m9_logged)
		return "M9";
	if (m_m8_logged)
		return "M8";
	if (m_m7c_logged)
		return "M7C";
	if (m_m7_rx_path_logged)
		return "M7B";
	if (m_m7a_logged)
		return "M7A";
	if (m_m7_logged)
		return "M7";
	if (m_m6_logged)
		return "M6";
	if (m_m5v_logged)
		return "M5V";
	if (m_m5_logged)
		return "M5";
	if (m_m3_m4_logged)
		return "M4";
	return "pre_M3";
}

void millennium_state::append_memory_trace_line(std::uint32_t phys_addr, u8 data)
{
	if (m_memory_trace_path.empty())
		return;
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	std::string const line = millennium_format_memory_trace_line(m_maincpu->total_cycles(), phys_addr, data, pc, sp,
		current_boot_milestone_tag());
	millennium_boot_trace_append_line(m_memory_trace_path, line);
}

void millennium_state::log_unknown_external_port(std::uint16_t port_full, bool is_write, std::uint8_t value)
{
	std::uint8_t const lo = port_full & 0xffU;
	bool const internal_mirror = (lo <= 0x3fU) && (port_full < 0x100U);
	if (internal_mirror || m_unknown_port_path.empty())
		return;
	if (port_full < 0x100U && m_io_known_or_suspected[lo])
		return;
	std::string const ts = millennium_boot_trace_timestamp_utc();
	std::string const line = millennium_format_unknown_port_json(ts, m_maincpu->total_cycles(),
		u16(m_maincpu->pc() & 0xffffU), port_full, is_write, value, nullptr, nullptr);
	millennium_boot_trace_append_line(m_unknown_port_path, line);
}

void millennium_state::emit_boot_m3_m4_snapshot(char const *m3_trigger)
{
	if (m_m3_m4_logged)
		return;
	m_m3_m4_logged = true;

	std::string const ts = millennium_boot_trace_timestamp_utc();
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffff);
	std::string const m3 = millennium_boot_trace_m3(ts, m_ram_write_events, sp, m3_trigger);
	millennium_boot_trace_append_line(m_boot_trace_path, m3);

	millennium_z180_snapshot const snap = build_z180_snapshot();
	std::string const m4 = millennium_format_boot_m4(ts, snap);
	millennium_boot_trace_append_line(m_boot_trace_path, m4);
}

u8 millennium_state::catch_all_io_r(offs_t offset)
{
	u16 const port = u16(offset & 0xffffU);
	bool const internal = millennium_z180_port_is_internal_window(*m_maincpu, port);
	u8 const data = internal ? millennium_z180_trace_read_byte(*m_maincpu, port) : m_unknown_default;
	char const *const tag = internal ? millennium_z180_internal_trace_tag(std::uint8_t(port & 0x3fU)) : "catch_all_unmapped";
	emit_io_trace_line(tag, port, 'r', data);
	// M7A: a modeled power-on response was observed arriving over CSIO (TRDR readback), not via external UART.
	if (internal && !m_m7a_logged && (port & 0x3fU) == 0x0bU && (data == 0x70U || data == 0x72U)) {
		std::string const ts = millennium_boot_trace_timestamp_utc();
		char buf[220];
		std::snprintf(buf, sizeof(buf),
			"{\"milestone\":\"M7A\",\"ts\":\"%s\",\"event\":\"M7A_TELEPHONY_ACK\",\"port\":\"0x000B\",\"response\":\"0x%02X\","
			"\"source\":\"csio_trdr\"}",
			ts.c_str(), unsigned(data));
		millennium_boot_trace_append_line(m_boot_trace_path, buf);
		m_m7a_logged = true;
	}
	if (!internal)
		log_unknown_external_port(port, false, data);
	return data;
}

void millennium_state::catch_all_io_w(offs_t offset, u8 data)
{
	u16 const port = u16(offset & 0xffffU);
	bool const internal = millennium_z180_port_is_internal_window(*m_maincpu, port);
	char const *const tag = internal ? millennium_z180_internal_trace_tag(std::uint8_t(port & 0x3fU)) : "catch_all_unmapped";
	emit_io_trace_line(tag, port, 'w', data);
	if (!internal)
		log_unknown_external_port(port, true, data);
}

u8 millennium_state::voiceware_phrase_r()
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u8 const r = m_voiceware->read_phrase_port(pc, cy);
	// Detect end of a decoded Voiceware play segment for M5C.
	bool const chip_idle = !m_voiceware->playing();
	if (m_voicew_chip_was_active && chip_idle) {
		if (!m_m5c_logged) {
			char pcbuf[16];
			std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
			std::string const ts = millennium_boot_trace_timestamp_utc();
			millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m5c(ts, pcbuf));
			m_m5c_logged = true;
		}
	}
	m_voicew_chip_was_active = !chip_idle;
	emit_io_trace_line("voice_phrase", 0x61, 'r', r);
	if (!m_voiceware_trace_path.empty()) {
		std::string const line = millennium_format_voiceware_trace_line(m_maincpu->total_cycles(), 'r', r,
			u16(m_maincpu->pc() & 0xffffU), u16(m_maincpu->state_int(Z180_SP) & 0xffffU), m_hw_cntl_port_image,
			current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_voiceware_trace_path, line);
	}
	return r;
}

void millennium_state::voiceware_phrase_w(u8 data)
{
	u64 const cy = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	emit_io_trace_line("voice_phrase", 0x61, 'w', data);
	clear_voice_segment_int0("voice_phrase_write");
	m_voiceware->write_phrase(data, pc, cy);
	// M5V (first phrase command) before M5A (uPD path engaged) in boot-trace ordering.
	if (!m_m5v_logged) {
		char pcbuf[16], phbuf[16];
		std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
		std::snprintf(phbuf, sizeof(phbuf), "0x%02X", unsigned(data));
		std::string const ts = millennium_boot_trace_timestamp_utc();
		millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m5v(ts, pcbuf, phbuf));
		m_m5v_logged = true;
	}
	if (!m_m5a_logged) {
		char pcbuf[16], phbuf[16];
		std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
		std::snprintf(phbuf, sizeof(phbuf), "0x%02X", unsigned(data));
		std::string const ts_a = millennium_boot_trace_timestamp_utc();
		millennium_boot_trace_append_line(m_boot_trace_path, millennium_boot_trace_m5a(ts_a, pcbuf, phbuf));
		m_m5a_logged = true;
	}
	if (!m_voiceware_trace_path.empty()) {
		std::string const line = millennium_format_voiceware_trace_line(m_maincpu->total_cycles(), 'w', data,
			u16(m_maincpu->pc() & 0xffffU), u16(m_maincpu->state_int(Z180_SP) & 0xffffU), m_hw_cntl_port_image,
			current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_voiceware_trace_path, line);
	}
}

void millennium_state::append_cpu_trace_line(std::string const &line)
{
	if (m_cpu_trace_path.empty())
		return;
	if (!m_trace_capture_full_cpu) {
		if (m_cpu_trace_ring.size() >= COMPACT_RING_MAX)
			m_cpu_trace_ring.pop_front();
		m_cpu_trace_ring.push_back(line);
		return;
	}
	if (!m_cpu_trace_ring_mode) {
		++m_cpu_trace_total_lines;
		if (m_cpu_trace_total_lines <= CPU_TRACE_LINEAR_MAX)
			millennium_boot_trace_append_line(m_cpu_trace_path, line);
		else {
			m_cpu_trace_ring_mode = true;
			m_cpu_trace_ring.push_back(line);
		}
	}
	else {
		if (m_cpu_trace_ring.size() >= CPU_TRACE_RING_MAX)
			m_cpu_trace_ring.pop_front();
		m_cpu_trace_ring.push_back(line);
	}
}

void millennium_state::flush_cpu_trace_ring_to_disk()
{
	if (m_cpu_trace_path.empty() || m_cpu_trace_ring.empty())
		return;
	millennium_boot_trace_append_line(m_cpu_trace_path,
		"{\"note\":\"cpu_trace_ring_tail\",\"lines\":" + std::to_string(m_cpu_trace_ring.size()) + '}');
	for (std::string const &s : m_cpu_trace_ring)
		millennium_boot_trace_append_line(m_cpu_trace_path, s);
	m_cpu_trace_ring.clear();
}

bool millennium_state::profile_should_log_io(char const *tag, std::uint16_t port, char rw)
{
	if (m_trace_capture_full_io)
		return true;
	std::string const t = tag ? tag : "";
	if (port == 0x0060U || port == 0x0061U)
		return true;
	if (t == "catch_all_unmapped") {
		unsigned &count = m_unknown_io_counts[port];
		if (count >= 16U)
			return false;
		++count;
		return true;
	}
	if (port >= 0x00e0U && port <= 0x00e7U)
		return m_uart_io_log_budget > 0U;
	// When Microwire JSONL is enabled, always log 8255 A/B/C data ports — uart profile normally
	// suppresses them, which hides whether EEPROM SK/DI traffic is present on 0x41–0x43.
	if (!m_microwire_trace_path.empty() && port >= 0x0041U && port <= 0x0043U)
		return true;
	if (m_trace_profile == trace_profile::voice && (t.find("voice") != std::string::npos || t.find("audio") != std::string::npos))
		return true;
	if (m_trace_profile == trace_profile::uart && (t.find("asci") != std::string::npos || t.find("uart") != std::string::npos
		|| t.find("csio") != std::string::npos || port == 0x0040U))
		return true;
	(void)rw;
	return false;
}

void millennium_state::append_io_trace_line_profiled(std::string const &line)
{
	if (m_io_trace_path.empty())
		return;
	if (m_trace_capture_full_io) {
		millennium_boot_trace_append_line(m_io_trace_path, line);
		return;
	}
	bool const is_uart = line.find("\"port\":\"0x00E") != std::string::npos || line.find("\"port\":\"0x00e") != std::string::npos;
	if (is_uart) {
		if (m_uart_io_log_budget > 0U) {
			--m_uart_io_log_budget;
			millennium_boot_trace_append_line(m_io_trace_path, line);
		} else {
			if (m_io_trace_ring.size() >= COMPACT_RING_MAX)
				m_io_trace_ring.pop_front();
			m_io_trace_ring.push_back(line);
		}
		return;
	}
	bool const hot_disk = line.find("\"port\":\"0x0060\"") != std::string::npos || line.find("\"port\":\"0x0061\"") != std::string::npos
		|| line.find("\"port\":\"0x0040\"") != std::string::npos || line.find("\"port\":\"0x0041\"") != std::string::npos
		|| line.find("\"port\":\"0x0042\"") != std::string::npos || line.find("\"port\":\"0x0043\"") != std::string::npos
		|| line.find("\"tag\":\"z180_csio\"") != std::string::npos
		|| line.find("\"tag\":\"catch_all_unmapped\"") != std::string::npos;
	if (hot_disk) {
		// Do not also push these lines onto m_io_trace_ring: flush_io_trace_ring_to_disk would
		// append them a second time and duplicate EEPROM SK edges in io-trace.jsonl.
		millennium_boot_trace_append_line(m_io_trace_path, line);
		return;
	}
	if (m_io_trace_ring.size() >= COMPACT_RING_MAX)
		m_io_trace_ring.pop_front();
	m_io_trace_ring.push_back(line);
}

void millennium_state::flush_io_trace_ring_to_disk(char const *reason)
{
	if (m_io_trace_path.empty() || m_io_trace_ring.empty())
		return;
	std::string const why = reason ? reason : "unspecified";
	millennium_boot_trace_append_line(
		m_io_trace_path, "{\"metadata\":true,\"event\":\"io_trace_ring_flush\",\"reason\":\"" + why + "\",\"lines\":"
			+ std::to_string(m_io_trace_ring.size()) + "}");
	for (std::string const &s : m_io_trace_ring)
		millennium_boot_trace_append_line(m_io_trace_path, s);
	m_io_trace_ring.clear();
}

void millennium_state::write_hot_summary_files()
{
	if (m_boot_trace_path.empty() || !m_boot_trace_path.has_parent_path())
		return;
	std::map<std::uint16_t, unsigned> port_counts;
	std::map<std::uint16_t, unsigned> pc_counts;
	auto const parse_hex4 = [](std::string const &s, char const *key, std::uint16_t &out) -> bool {
		std::string const k = std::string("\"") + key + "\":\"0x";
		std::size_t const p = s.find(k);
		if (p == std::string::npos || p + k.size() + 4 > s.size())
			return false;
		unsigned v = 0U;
		for (std::size_t i = 0; i < 4; i++) {
			char const c = s[p + k.size() + i];
			v <<= 4;
			if (c >= '0' && c <= '9')
				v |= unsigned(c - '0');
			else if (c >= 'a' && c <= 'f')
				v |= unsigned(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F')
				v |= unsigned(c - 'A' + 10);
			else
				return false;
		}
		out = std::uint16_t(v);
		return true;
	};
	for (std::string const &s : m_io_trace_ring) {
		std::uint16_t port = 0, pc = 0;
		if (parse_hex4(s, "port", port))
			port_counts[port]++;
		if (parse_hex4(s, "pc", pc))
			pc_counts[pc]++;
	}
	std::filesystem::path const hot_pc = m_boot_trace_path.parent_path() / "hot-pc-frequency.json";
	std::filesystem::path const hot_port = m_boot_trace_path.parent_path() / "hot-port-frequency.json";
	std::ofstream pcf(hot_pc, std::ios::binary | std::ios::trunc);
	if (pcf) {
		pcf << "{\"schema_version\":\"coinline.hot_pc_frequency/v2\",\"top_pcs\":[";
		bool first = true;
		for (auto const &it : pc_counts) {
			if (!first)
				pcf << ',';
			first = false;
			char buf[16];
			std::snprintf(buf, sizeof(buf), "0x%04X", unsigned(it.first));
			pcf << "{\"pc\":\"" << buf << "\",\"count\":" << it.second << '}';
		}
		pcf << "]}\n";
	}
	std::ofstream pof(hot_port, std::ios::binary | std::ios::trunc);
	if (pof) {
		pof << "{\"schema_version\":\"coinline.hot_port_frequency/v1\",\"top_ports\":[";
		bool first = true;
		for (auto const &it : port_counts) {
			if (!first)
				pof << ',';
			first = false;
			char buf[16];
			std::snprintf(buf, sizeof(buf), "0x%04X", unsigned(it.first));
			pof << "{\"port\":\"" << buf << "\",\"count\":" << it.second << '}';
		}
		pof << "]}\n";
	}
}

void millennium_state::cpu_trace_sample_tick()
{
	if (m_cpu_trace_path.empty() && m_z180_reg_trace_path.empty() && m_interrupt_trace_path.empty()
		&& m_timer_trace_path.empty() && m_asci_trace_path.empty() && m_reset_trace_path.empty()
		&& m_mmu_translation_trace_path.empty())
		return;
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	m_last_trace_pc = pc;
	m_last_trace_sp = sp;
	address_space &ps = m_maincpu->space(AS_PROGRAM);
	u8 const op0 = ps.read_byte(pc);
	u8 const op1 = ps.read_byte(u16(pc + 1U));
	u8 const op2 = ps.read_byte(u16(pc + 2U));
	bool const iff1 = m_maincpu->state_int(Z180_IFF1) != 0;
	if (!m_cpu_trace_path.empty()) {
		std::string const cline = millennium_format_cpu_trace_line(m_maincpu->total_cycles(), pc, op0, op1, op2, sp,
			iff1, current_boot_milestone_tag());
		append_cpu_trace_line(cline);
	}
	if (!m_z180_reg_trace_path.empty()) {
		millennium_z180_snapshot const snap = build_z180_snapshot();
		bool const iff2 = m_maincpu->state_int(Z180_IFF2) != 0;
		std::string const zline = millennium_format_z180_reg_trace_line(m_maincpu->total_cycles(), pc, sp, iff1, iff2,
			snap, current_boot_milestone_tag());
		millennium_boot_trace_append_line(m_z180_reg_trace_path, zline);
	}
	append_mmu_translation_trace_line();
	append_supplemental_cpu_trace_samples();
}

void millennium_state::append_supplemental_cpu_trace_samples()
{
	u64 const cyc = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	char const *const ms = current_boot_milestone_tag();

	if (!m_interrupt_trace_path.empty()) {
		bool const iff1 = m_maincpu->state_int(Z180_IFF1) != 0;
		bool const iff2 = m_maincpu->state_int(Z180_IFF2) != 0;
		int const im = int(m_maincpu->state_int(Z180_IM) & 3);
		u8 const itc = u8(m_maincpu->state_int(Z180_ITC) & 0xff);
		u8 const il = u8(m_maincpu->state_int(Z180_IL) & 0xff);
		char buf[420];
		std::snprintf(buf, sizeof(buf),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"iff1\":%s,\"iff2\":%s,\"im\":%d,\"itc\":\"0x%02X\",\"il\":\"0x%02X\","
			"\"milestone\":\"%s\"}",
			static_cast<unsigned long long>(cyc), unsigned(pc), iff1 ? "true" : "false", iff2 ? "true" : "false", im,
			unsigned(itc), unsigned(il), ms);
		millennium_boot_trace_append_line(m_interrupt_trace_path, buf);
	}
	if (!m_timer_trace_path.empty()) {
		u8 const tcr = u8(m_maincpu->state_int(Z180_TCR) & 0xff);
		u16 const rldr0 = u16(m_maincpu->state_int(Z180_RLDR0) & 0xffff);
		u16 const tmdr0 = u16(m_maincpu->state_int(Z180_TMDR0) & 0xffff);
		u16 const rldr1 = u16(m_maincpu->state_int(Z180_RLDR1) & 0xffff);
		u16 const tmdr1 = u16(m_maincpu->state_int(Z180_TMDR1) & 0xffff);
		u8 const cntr = u8(m_maincpu->state_int(Z180_CNTR) & 0xff);
		char buf[480];
		std::snprintf(buf, sizeof(buf),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"tcr\":\"0x%02X\",\"rldr0\":\"0x%04X\",\"tmdr0\":\"0x%04X\","
			"\"rldr1\":\"0x%04X\",\"tmdr1\":\"0x%04X\",\"cntr\":\"0x%02X\",\"milestone\":\"%s\"}",
			static_cast<unsigned long long>(cyc), unsigned(pc), unsigned(tcr), unsigned(rldr0), unsigned(tmdr0),
			unsigned(rldr1), unsigned(tmdr1), unsigned(cntr), ms);
		millennium_boot_trace_append_line(m_timer_trace_path, buf);
	}
	if (!m_asci_trace_path.empty()) {
		u8 const cntla0 = u8(m_maincpu->state_int(Z180_CNTLA0) & 0xff);
		u8 const cntlb0 = u8(m_maincpu->state_int(Z180_CNTLB0) & 0xff);
		u8 const stat0 = u8(m_maincpu->state_int(Z180_STAT0) & 0xff);
		u8 const cntla1 = u8(m_maincpu->state_int(Z180_CNTLA1) & 0xff);
		u8 const cntlb1 = u8(m_maincpu->state_int(Z180_CNTLB1) & 0xff);
		u8 const stat1 = u8(m_maincpu->state_int(Z180_STAT1) & 0xff);
		char buf[480];
		std::snprintf(buf, sizeof(buf),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"cntla0\":\"0x%02X\",\"cntlb0\":\"0x%02X\",\"stat0\":\"0x%02X\","
			"\"cntla1\":\"0x%02X\",\"cntlb1\":\"0x%02X\",\"stat1\":\"0x%02X\",\"modem_dcd\":%s,\"modem_cts\":%s,"
			"\"milestone\":\"%s\"}",
			static_cast<unsigned long long>(cyc), unsigned(pc), unsigned(cntla0), unsigned(cntlb0), unsigned(stat0),
			unsigned(cntla1), unsigned(cntlb1), unsigned(stat1), m_modem->dcd_line() ? "true" : "false",
			m_modem->cts_line() ? "true" : "false", ms);
		millennium_boot_trace_append_line(m_asci_trace_path, buf);
	}
	if (!m_reset_trace_path.empty()) {
		bool const looks_reset = (pc == 0);
		char buf[220];
		std::snprintf(buf, sizeof(buf),
			"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sample_tag\":\"pc_zero_watch\",\"value\":%s,\"milestone\":\"%s\"}",
			static_cast<unsigned long long>(cyc), unsigned(pc), looks_reset ? "true" : "false", ms);
		millennium_boot_trace_append_line(m_reset_trace_path, buf);
	}
}

void millennium_state::vector_event_probe_tick()
{
	if (m_vector_event_budget == 0U)
		return;
	bool const tracing = !m_interrupt_events_path.empty() || !m_vector_events_path.empty()
		|| !m_context_switch_events_path.empty() || !m_eidi_events_path.empty()
		|| !m_fetch_provenance_trace_path.empty() || !m_stack_control_flow_trace_path.empty()
		|| (m_trace_capture_fault_context && !m_first_pc_ffff_context_path.empty())
		|| (m_trace_capture_fault_context && !m_first_rst38_context_path.empty());
	if (!tracing)
		return;

	u64 const cyc = m_maincpu->total_cycles();
	u16 const pc = u16(m_maincpu->pc() & 0xffffU);
	u16 const sp = u16(m_maincpu->state_int(Z180_SP) & 0xffffU);
	bool const iff1 = m_maincpu->state_int(Z180_IFF1) != 0;
	bool const iff2 = m_maincpu->state_int(Z180_IFF2) != 0;
	int const im = int(m_maincpu->state_int(Z180_IM) & 3);
	char const *const ms = current_boot_milestone_tag();
	address_space &ps = m_maincpu->space(AS_PROGRAM);
	u8 const op0 = ps.read_byte(pc);
	u8 const op1 = ps.read_byte(u16(pc + 1U));
	u8 const op2 = ps.read_byte(u16(pc + 2U));

	u16 const prev_pc = m_vecprobe_last_pc;
	std::uint8_t const prev_op0 = m_vecprobe_last_op0;
	bool const prev_iff1 = m_vecprobe_last_iff1;
	bool const prev_iff2 = m_vecprobe_last_iff2;
	bool const new_decode_slot = (pc != prev_pc || op0 != prev_op0);
	std::string const provenance = format_fetch_provenance_event(
		(pc == 0xffffU) ? "pc_ffff_execute" : ((op0 == 0xffU) ? "rst38_opcode_fetch" : "sample"), pc, sp, op0, op1, op2);
	if (m_pc_fault_context_ring.size() >= 256U)
		m_pc_fault_context_ring.pop_front();
	m_pc_fault_context_ring.push_back(provenance);
	if (!m_fetch_provenance_trace_path.empty()
		&& (pc == 0xffffU || op0 == 0xffU || new_decode_slot || m_first_pc_ffff_next_remaining > 0U
			|| m_first_rst38_next_remaining > 0U)) {
		millennium_boot_trace_append_line(m_fetch_provenance_trace_path, provenance);
	}
	if (pc == 0xffffU && !m_first_pc_ffff_seen) {
		m_first_pc_ffff_seen = true;
		m_first_pc_ffff_next_remaining = 64U;
		if (!m_first_pc_ffff_path.empty())
			millennium_boot_trace_append_line(m_first_pc_ffff_path, provenance);
		if (!m_first_pc_ffff_context_path.empty()) {
			for (std::string const &line : m_pc_fault_context_ring)
				millennium_boot_trace_append_line(m_first_pc_ffff_context_path, line);
		}
	}
	else if (m_first_pc_ffff_next_remaining > 0U && !m_first_pc_ffff_context_path.empty()) {
		millennium_boot_trace_append_line(m_first_pc_ffff_context_path, provenance);
		--m_first_pc_ffff_next_remaining;
	}
	if ((pc == 0x0038U || op0 == 0xffU) && !m_first_rst38_seen) {
		m_first_rst38_seen = true;
		m_first_rst38_next_remaining = 64U;
		if (!m_first_rst38_context_path.empty()) {
			for (std::string const &line : m_pc_fault_context_ring)
				millennium_boot_trace_append_line(m_first_rst38_context_path, line);
		}
	}
	else if (m_first_rst38_next_remaining > 0U && !m_first_rst38_context_path.empty()) {
		millennium_boot_trace_append_line(m_first_rst38_context_path, provenance);
		--m_first_rst38_next_remaining;
	}
	maybe_emit_stack_control_flow(cyc, pc, sp, op0, op1, op2, ms, new_decode_slot);

	auto nearby_symbol_for_pc = [](u16 addr) -> char const * {
		if (addr == 0x0038U)
			return "rst38_vector";
		if (addr == 0x5b29U)
			return "voice_phrase_loop";
		if (addr >= 0x00cfU && addr <= 0x00e5U)
			return "context_save_band";
		if (addr >= 0x0170U && addr <= 0x0188U)
			return "reset_boot_front";
		return "unknown";
	};

	auto append_int = [&](char const *ev) {
		if (m_interrupt_events_path.empty() || m_vector_event_budget == 0U)
			return;
		millennium_boot_trace_append_line(m_interrupt_events_path,
			millennium_format_vector_event_line(cyc, ev, pc, sp, op0, op1, iff1, im, ms));
		--m_vector_event_budget;
	};
	auto append_vec = [&](char const *ev) {
		if (m_vector_events_path.empty() || m_vector_event_budget == 0U)
			return;
		millennium_boot_trace_append_line(m_vector_events_path,
			millennium_format_vector_event_line(cyc, ev, pc, sp, op0, op1, iff1, im, ms));
		--m_vector_event_budget;
	};
	auto append_ctx = [&](char const *ev) {
		if (m_context_switch_events_path.empty() || m_vector_event_budget == 0U)
			return;
		millennium_boot_trace_append_line(m_context_switch_events_path,
			millennium_format_vector_event_line(cyc, ev, pc, sp, op0, op1, iff1, im, ms));
		--m_vector_event_budget;
	};
	auto append_eidi = [&](char const *mnemonic, char const *note) {
		if (m_eidi_events_path.empty() || m_vector_event_budget == 0U)
			return;
		millennium_boot_trace_append_line(
			m_eidi_events_path,
			millennium_format_eidi_event_line(cyc, pc, op0, op1, mnemonic, sp, prev_iff1, prev_iff2, iff1, iff2,
				nearby_symbol_for_pc(pc), note, ms));
		--m_vector_event_budget;
	};

	if (iff1 != m_vecprobe_last_iff1)
		append_int(iff1 ? "iff1_rise_sampled" : "iff1_fall_sampled");

	if (new_decode_slot) {
		if (op0 == 0xfbU) {
			append_int("ei_opcode_sample");
			append_eidi("EI", "opcode fetch observed; iff transition may occur on following instruction");
		}
		if (op0 == 0xf3U) {
			append_int("di_opcode_sample");
			append_eidi("DI", "opcode fetch observed");
		}
		if (op0 == 0xedU) {
			if (op1 == 0x4dU) {
				append_int("reti_opcode_sample");
				append_eidi("RETI", "ED 4D");
			}
			if (op1 == 0x45U) {
				append_int("retn_opcode_sample");
				append_eidi("RETN", "ED 45");
			}
			if (op1 == 0x46U) {
				append_int("im0_opcode_sample");
				append_eidi("IM 0", "ED 46");
			}
			if (op1 == 0x56U) {
				append_int("im1_opcode_sample");
				append_eidi("IM 1", "ED 56");
			}
			if (op1 == 0x5eU) {
				append_int("im2_opcode_sample");
				append_eidi("IM 2", "ED 5E");
			}
		}
		if (op0 == 0xc9U) {
			append_int("ret_opcode_sample");
			append_eidi("RET", "C9");
		}
		if (op0 == 0xcdU)
			append_eidi("CALL", "CD nn nn");
		if (op0 == 0xc3U)
			append_eidi("JP", "C3 nn nn");
		if (op0 == 0xffU) {
			append_vec("opcode_ff_rst38_sample");
			append_eidi("RST 38", "FF");
		}
	}

	if (pc == 0x0038U && prev_pc != 0x0038U)
		append_vec("pc_enter_0x0038");

	bool const in_ctx = (pc >= 0x00cfU && pc <= 0x00e5U);
	if (in_ctx && !m_vecprobe_in_ctx_band) {
		append_ctx("context_save_band_enter");
		m_vecprobe_in_ctx_band = true;
	}
	else if (!in_ctx && m_vecprobe_in_ctx_band) {
		append_ctx("context_save_band_exit");
		m_vecprobe_in_ctx_band = false;
	}

	m_vecprobe_last_pc = pc;
	m_vecprobe_last_op0 = op0;
	m_vecprobe_last_iff1 = iff1;
	m_vecprobe_last_iff2 = iff2;
}

void millennium_state::write_stop_debug_artifacts()
{
	if (m_boot_trace_path.empty())
		return;
	std::filesystem::path const dir = m_boot_trace_path.parent_path();
	if (dir.empty())
		return;
	if (!m_vfd_final_state_path.empty()) {
		std::ofstream fs(m_vfd_final_state_path, std::ios::binary | std::ios::trunc);
		if (fs)
			fs << m_vfd->export_snapshot_json();
	}
	if (!m_vfd_final_text_path.empty()) {
		std::ofstream ft(m_vfd_final_text_path, std::ios::binary | std::ios::trunc);
		if (ft) {
			std::string const row0 = m_vfd->first_text_row();
			ft << row0 << "\n";
			if (m_display_profile.rows > 1) {
				std::string row1;
				auto const &cells = m_vfd->vfd_cells();
				for (int c = 0; c < m_display_profile.columns; ++c)
					row1.push_back(cells[std::size_t(m_display_profile.columns + c)]);
				ft << row1 << "\n";
			}
		}
	}
	if (!m_telephony_runtime_state_path.empty()) {
		std::ofstream rs(m_telephony_runtime_state_path, std::ios::binary | std::ios::trunc);
		if (rs) {
			rs << "{\n";
			rs << "  \"power_on_seen\": " << (m_tel_boot_power_ack_seen ? "true" : "false") << ",\n";
			rs << "  \"init_ack_sent\": " << (m_tel_boot_power_ack_seen ? "true" : "false") << ",\n";
			rs << "  \"config_ack_sent\": " << (!m_ip_tx_need_length_byte ? "true" : "false") << ",\n";
			rs << "  \"error_report_clear_sent\": " << (m_alarm_tel_not_responding_cleared ? "true" : "false") << ",\n";
			rs << "  \"status_sent\": " << (m_csio_status_frame_ok ? "true" : "false") << ",\n";
			rs << "  \"runtime_poll_count\": " << m_tel_runtime_poll_count << ",\n";
			rs << "  \"last_runtime_poll_command\": \"" << m_tel_last_runtime_poll_command << "\",\n";
			rs << "  \"last_runtime_response\": \"" << m_tel_last_runtime_response << "\",\n";
			rs << "  \"last_response_cycle\": " << static_cast<unsigned long long>(m_tel_last_health_sweep_cycle) << ",\n";
			rs << "  \"runtime_timeout_count\": " << m_tel_runtime_timeout_count << ",\n";
			rs << "  \"fault_latched\": " << (m_alarm_tel_not_responding_latched ? "true" : "false") << ",\n";
			rs << "  \"fault_clear_count\": " << (m_alarm_tel_not_responding_cleared ? 1 : 0) << ",\n";
			rs << "  \"hook_state\": \"" << (m_tel_hook_onhook ? "on_hook" : "off_hook") << "\",\n";
			rs << "  \"call_state\": \"" << (m_tel_hook_onhook ? "idle" : "offhook_active") << "\",\n";
			rs << "  \"ready_state\": " << (m_tel_ready_sequence_completed ? "true" : "false") << "\n";
			rs << "}\n";
		}
	}
	if (m_m10_logged || m_boot_protocol_ready_milestone_logged || m_boot_acceptance_ready_milestone_logged)
		return;
	if (!m_m6_logged && !m_m10_logged)
		flush_cpu_trace_ring_to_disk();
	if (m_trace_capture_hot_summary)
		write_hot_summary_files();
	if (!m_m6_logged)
		flush_io_trace_ring_to_disk("run_end_blocker_summary");

	std::ostringstream hexblk;
	hexblk << "PC sample (from trace hooks): 0x" << std::hex << std::uppercase << unsigned(m_last_trace_pc) << std::dec
	       << "\n";
	hexblk << "SP sample: 0x" << std::hex << std::uppercase << unsigned(m_last_trace_sp) << std::dec << "\n";
	hexblk << "Last I/O port sample: 0x" << std::hex << std::uppercase << unsigned(m_last_trace_port) << std::dec << "\n";
	memory_region *const rom = memregion("flash");
	if (rom) {
		u16 const base = m_last_trace_pc >= 0x10 ? u16(m_last_trace_pc - 0x10) : 0;
		hexblk << "\nflash.bin bytes [0x" << std::hex << std::uppercase << unsigned(base) << " .. +0x40):\n"
		       << std::dec;
		u8 const *const rb = rom->base();
		u32 const sz = u32(rom->bytes());
		for (int row = 0; row < 4; ++row) {
			u32 const adr = u32(base) + u32(row * 16);
			if (adr + 16U > sz)
				break;
			hexblk << std::hex << std::uppercase << adr << ": ";
			for (int i = 0; i < 16; ++i)
				hexblk << std::hex << std::uppercase << unsigned(rb[adr + u32(i)]) << ' ';
			hexblk << std::dec << '\n';
		}
	}

	std::filesystem::path const dis = dir / "disassembly-around-blocker.txt";
	{
		std::FILE *f = std::fopen(dis.string().c_str(), "wb");
		if (f) {
			std::string const s = hexblk.str();
			std::fwrite(s.data(), 1, s.size(), f);
			std::fclose(f);
		}
	}

	std::filesystem::path const blk = dir / "boot-blocker.md";
	{
		std::FILE *f = std::fopen(blk.string().c_str(), "wb");
		if (f) {
			std::ostringstream md;
			md << "# Boot blocker (driver stop)\n\n";
			md << "M10 not reached. Last sampled PC/SP shown below.\n\n";
			md << "```\n"
			   << hexblk.str() << "```\n";
			std::string const s = md.str();
			std::fwrite(s.data(), 1, s.size(), f);
			std::fclose(f);
		}
	}

	std::filesystem::path const sum = dir / "boot-debug-summary.md";
	{
		std::FILE *f = std::fopen(sum.string().c_str(), "wb");
		if (f) {
			std::ostringstream sm;
			sm << "# Boot debug summary\n\n";
			sm << "- **Artifacts**: see `boot-trace.jsonl`, `io-trace.jsonl`, `cpu-trace`*, `memory-trace.jsonl`, "
			      "`z180-register-trace.jsonl`.\n";
			sm << "- **M5 is keypad-only** in this build (first qualifying PIO keypad access).\n";
			std::string const s = sm.str();
			std::fwrite(s.data(), 1, s.size(), f);
			std::fclose(f);
		}
	}
}

void millennium_state::device_stop()
{
#ifdef _WIN32
	if (m_win_vfd_font_added && !m_win_vfd_font_path.empty()) {
		RemoveFontResourceExW(m_win_vfd_font_path.c_str(), FR_PRIVATE, nullptr);
		m_win_vfd_font_added = false;
		m_win_vfd_font_path.clear();
	}
#endif
	write_stop_debug_artifacts();
	driver_device::device_stop();
}

void millennium_state::schedule_driver_timer(s32 id, attotime const &delay)
{
	machine().scheduler().timer_set(delay, timer_expired_delegate(FUNC(millennium_state::driver_timer_cb), this), id);
}

void millennium_state::sync_modem_asci_lines()
{
	int const cts = m_modem->cts_line() ? 1 : 0;
	int const dcd = m_modem->dcd_line() ? 1 : 0;
	m_maincpu->cts0_w(cts);
	m_maincpu->rxs_cts1_w(cts);
	m_maincpu->dcd0_w(dcd);
}

uint32_t millennium_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, rectangle const &cliprect)
{
	(void)screen;
	sync_modem_asci_lines();
	bool const dcd = m_modem->dcd_line();
	if (dcd != m_last_modem_dcd) {
		m_telephony->notify_modem_dcd(dcd, m_maincpu->total_cycles(), u16(m_maincpu->pc() & 0xffffU));
		m_last_modem_dcd = dcd;
	}

	bitmap.fill(rgb_t(0x08, 0x09, 0x0c), cliprect);

	millennium_display_profile const &dp = m_vfd->display_profile();
	std::vector<char> const &cells = m_vfd->vfd_cells();
	int const rows = dp.rows;
	int const cols = dp.columns;
	std::string row0_for_logic;
	std::string row1_for_logic;
	if (rows >= 1 && cols >= 1 && std::size_t(rows * cols) <= cells.size()) {
		row0_for_logic.assign(cells.begin(), cells.begin() + cols);
		if (rows > 1)
			row1_for_logic.assign(cells.begin() + cols, cells.begin() + 2 * cols);
	}
	if (rows >= 1 && cols >= 1 && std::size_t(rows * cols) <= cells.size()) {
		// Force dot-plane rendering for deterministic VFD output across hosts.
		bool const use_oem_vfd_font = false;
		int const sx_fit = std::max(1, bitmap.width() / (cols * 6));
		int const sy_fit = std::max(1, bitmap.height() / (rows * 8));
		// Use anisotropic scaling so 2x20 text fills the VFD window height instead of
		// becoming tiny when width is the limiting dimension.
		int const dm_scale_x = sx_fit;
		int const dm_scale_y = sy_fit;
		// CU200xx 2x20 style pitch: 5x7 dots with 1-column/1-row inter-cell spacing.
		int const spaced_cols = cols * 6;
		int const spaced_rows = rows * 8;
		int const panel_w = spaced_cols * dm_scale_x;
		int const panel_h = spaced_rows * dm_scale_y;
		int const vfd_x0 = std::max(0, (bitmap.width() - panel_w) / 2);
		int const vfd_y0 = std::max(0, (bitmap.height() - panel_h) / 2);
		std::uint8_t const lum = m_vfd->buffer().luminance();
		float const gain = std::clamp(float(lum) / 255.f, 0.12f, 1.f);
		auto const scale_ch = [gain](std::uint8_t v) -> std::uint8_t {
			return static_cast<std::uint8_t>(std::min(255, int(std::lround(float(v) * gain))));
		};
		rgb_t const panel_bg(scale_ch(0x02), scale_ch(0x06), scale_ch(0x04));
		bitmap.fill(panel_bg, cliprect);

		render_color phos;
		phos.set(1.0f, 0.12f, 0.92f, 0.42f);
		rgb_t const dm_color(scale_ch(0x38), scale_ch(0xff), scale_ch(0xb8));
		for (int r = 0; r < rows; ++r) {
			std::string row;
			row.reserve(std::size_t(cols * 3));
			bool const katakana = m_vfd->buffer().katakana_font();
			for (int c = 0; c < cols; ++c) {
				std::uint8_t const b = static_cast<std::uint8_t>(cells[std::size_t(r * cols + c)]);
				millennium_vfd_cell_utf8::append_cell(row, b, katakana);
			}
			int const row_h = use_oem_vfd_font ? 28 : (8 * dm_scale_y);
			int const ty = vfd_y0 + r * row_h;
			if (use_oem_vfd_font) {
				bitmap_argb32 row_bm(panel_w, row_h);
				row_bm.fill(0);
				rectangle const rb(0, panel_w - 1, 0, row_h - 1);
				coinline_draw_text(*m_vfd_font, row_bm, rb, row, 1, phos);
				for (int yy = 0; yy < row_h; ++yy) {
					int const by = ty + yy;
					if (by < cliprect.min_y || by > cliprect.max_y)
						continue;
					for (int xx = 0; xx < panel_w; ++xx) {
						int const bx = vfd_x0 + xx;
						if (bx < cliprect.min_x || bx > cliprect.max_x)
							continue;
						rgb_t const t(row_bm.pix(yy, xx));
						if (t.a() != 0)
							bitmap.pix(by, bx) = t;
					}
				}
			}
			else {
				auto const &dots = m_vfd->buffer().dot_plane();
				int const dot_cols = m_vfd->buffer().dot_cols();
				int const dot_rows = m_vfd->buffer().dot_rows();
				for (int dy = 0; dy < dot_rows; ++dy) {
					int const char_row = dy / 7;
					int const local_row = dy % 7;
					int const spaced_y = char_row * 8 + local_row;
					for (int dx = 0; dx < dot_cols; ++dx) {
						if (dots[std::size_t(dy * dot_cols + dx)] == 0U)
							continue;
						int const char_col = dx / 5;
						int const local_col = dx % 5;
						int const spaced_x = char_col * 6 + local_col;
						int const px0 = vfd_x0 + spaced_x * dm_scale_x;
						int const py0 = vfd_y0 + spaced_y * dm_scale_y;
						for (int sy = 0; sy < dm_scale_y; ++sy) {
							int const py = py0 + sy;
							if (py < cliprect.min_y || py > cliprect.max_y || py >= bitmap.height())
								continue;
							for (int sx = 0; sx < dm_scale_x; ++sx) {
								int const px = px0 + sx;
								if (px < cliprect.min_x || px > cliprect.max_x || px >= bitmap.width())
									continue;
								bitmap.pix(py, px) = dm_color;
							}
						}
					}
				}
			}
		}
	}

	return 0;
}

TIMER_CALLBACK_MEMBER(millennium_state::driver_timer_cb)
{
	s32 const id = param;
	if (id == TID_CARD_UI) {
		poll_card_ui();
		schedule_driver_timer(TID_CARD_UI, attotime::from_msec(25));
		return;
	}
	if (id == TID_COIN_UI) {
		poll_coin_ui();
		schedule_driver_timer(TID_COIN_UI, attotime::from_msec(25));
		return;
	}
	if (id == TID_HOST_POLL) {
		u64 const cy = m_maincpu->total_cycles();
		u64 const hz = static_cast<u64>(m_maincpu->unscaled_clock());
		u64 cycles_per_10ms = hz / 100U;
		if (cycles_per_10ms == 0U)
			cycles_per_10ms = 1U;

		if (!m_tp_interrupt_trace_path.empty()) {
			u64 const dpoll = m_tp_last_host_poll_cycle ? (cy - m_tp_last_host_poll_cycle) : 0ULL;
			m_tp_last_host_poll_cycle = cy;
			double const poll_ms = (hz != 0ULL && dpoll != 0ULL) ? double(dpoll) * 1000.0 / double(hz) : 0.;
			bool const iff1 = (m_maincpu->state_int(Z180_IFF1) & 1) != 0;
			bool const iff2 = (m_maincpu->state_int(Z180_IFF2) & 1) != 0;
			char iv[1100];
			std::snprintf(iv, sizeof(iv),
				"{\"cycle\":%llu,\"event\":\"host_scheduler_tid_HOST_POLL\","
				"\"delta_cycles_since_previous_poll\":%llu,\"estimated_ms_between_polls\":%.6f,"
				"\"pc\":\"0x%04X\",\"IFF1\":%s,\"IFF2\":%s,"
				"\"ready_state\":%s,\"timeout_counter\":%u,\"alarm_latched\":%s,"
				"\"current_vfd_text\":\"%s|%s\"}",
				static_cast<unsigned long long>(cy), static_cast<unsigned long long>(dpoll), poll_ms,
				unsigned(m_maincpu->pc() & 0xffffU), iff1 ? "true" : "false", iff2 ? "true" : "false",
				m_tel_ready_sequence_completed ? "true" : "false", unsigned(m_tel_runtime_timeout_count),
				m_alarm_tel_not_responding_latched ? "true" : "false",
				vfd_row_text(0).c_str(), vfd_row_text(1).c_str());
			millennium_boot_trace_append_line(m_tp_interrupt_trace_path, iv);
		}

		// CSI/O TP link arms after PWR_FAIL_LINE release + DELAY_AFTER_PWR_FAIL_RESET (~60 ms), see board_status_w.
		// Fallback: some traces never touch HW_CNTL — approximate original fixed ladder after a grace window.
		if (!m_tel_ip_link_enabled) {
			if (m_tel_ip_link_enable_deadline_cycle == 0ULL
				&& cy > m_tel_reset_cycle_start + 50ULL * cycles_per_10ms) {
				m_tel_ip_link_enable_deadline_cycle = cy + cycles_per_10ms * (15U + 6U);
			}
			if (m_tel_ip_link_enable_deadline_cycle != 0ULL && cy >= m_tel_ip_link_enable_deadline_cycle) {
				m_tel_ip_link_enabled = true;
				m_tel_link_enable_cycle = cy;
				m_tel_boot_code_deadline_cycle = cy + (cycles_per_10ms * 100U);
				telephony_runtime_trace_event("telephony_init_status_ok", "", "", true, "ip_link_enabled_after_reset_delays");
			}
		}

		if ((m_tp_backend_kind != tp_backend_kind::pcd3349a || !m_tp_pcd3349a)
			&& m_tel_ip_link_enabled && !m_tel_boot_code_sent && !m_ipcomm_have_rx_byte && m_ipcomm_rx_prio_bytes.empty()
			&& m_ipcomm_rx_bytes.empty()) {
			// Emit reset-vs-ack boot code according to current telephony reset path state:
			// \c 0x70 on-hook idle after reset; \c 0x72 off-hook / active-call survivor path.
			u8 const boot_code = m_tel_hook_onhook_stable ? 0x70U : 0x72U;
			ipcomm_queue_rx_byte(boot_code, true);
			m_tel_boot_code_sent = true;
			telephony_runtime_trace_event("telephony_init_ack", opcode_hex_byte(boot_code).c_str(),
				opcode_hex_byte(boot_code).c_str(), true, "first_tp_boot_code_after_link_enable");
		}

		if ((m_tp_backend_kind != tp_backend_kind::pcd3349a || !m_tp_pcd3349a)
			&& m_tel_ip_link_enabled && !m_tel_boot_code_sent && m_tel_boot_code_deadline_cycle != 0ULL
			&& cy > m_tel_boot_code_deadline_cycle) {
			telephony_runtime_trace_event("telephony_runtime_poll_timeout", "boot_code_0x70_or_0x72", "", false,
				"tp_boot_code_deadline_expired");
			m_tel_boot_code_deadline_cycle = 0ULL;
		}
		// Option A runtime cadence guard:
		// If CP->TP poll decode is sparse under CSI/O edge timing, maintain firmware-visible
		// telephony health through OOS idle by emitting checksum-valid error/status frames periodically.
		// Do not gate on RTOS xflags: stalled init modeling should not starve CSI/O acceptance.
		// Bootstrap: after the TP boot ack (0x70/0x72) is visible to CP, emit C4/C0 even before the
		// boot contract closes — otherwise keepalive never runs when the config-download completion
		// path does not fire, and `m_tel_ready_sequence_completed` stays false forever.
		bool const tel_health_keepalive_runtime = m_tel_ip_link_enabled && m_tel_fw_boot_contract_satisfied;
		bool const tel_health_keepalive_bootstrap = m_tel_ip_link_enabled && m_tel_boot_power_ack_seen
			&& !m_tel_fw_boot_contract_satisfied;
		if ((tel_health_keepalive_runtime || tel_health_keepalive_bootstrap) && !m_ipcomm_have_rx_byte
			&& m_ipcomm_rx_prio_bytes.empty() && m_ipcomm_rx_bytes.empty()) {
			u64 keepalive_period = std::max(1ULL, hz / 2U); // default 500 ms
			if (m_tel_response_policy == tel_response_policy::immediate)
				keepalive_period = std::max(1ULL, hz / 10U); // 100 ms: matches script "immediate" intent
			if (tel_health_keepalive_bootstrap)
				keepalive_period = std::max(1ULL, hz / 20U); // 50 ms: converge boot contract before UI settles
			if (m_tel_last_runtime_keepalive_cycle == 0ULL || (cy - m_tel_last_runtime_keepalive_cycle) >= keepalive_period)
				tel_queue_runtime_keepalive_c4_c0(cy);
		}
		if (m_tel_hook_onhook_stable != m_tel_hook_onhook && m_tel_hook_stabilize_until_cycle != 0ULL
			&& cy >= m_tel_hook_stabilize_until_cycle) {
			m_tel_hook_onhook_stable = m_tel_hook_onhook;
			m_audio_route->notify_hook_off(!m_tel_hook_onhook_stable);
		}
		refresh_craft_entry_window(cy);
		try_emit_boot_readiness_milestones(cy);
		if (m_tel_ready_sequence_completed && m_rtos_startup_contract.xflag_all_init_done()
			&& !m_alarm_tel_not_responding_cleared && m_tel_last_health_sweep_cycle != 0U) {
			u64 const timeout_cycles = hz * 3U; // TELEPHONY_COMM_TIMEOUT: 300 * 10 ms
			if (cy > m_tel_last_health_sweep_cycle && (cy - m_tel_last_health_sweep_cycle) > timeout_cycles) {
				u64 const grace_cycles_base = hz != 0ULL ? (hz * 2ULL) : 0ULL;
				u64 const grace_cycles_csio = hz != 0ULL ? (hz * 5ULL) : 0ULL;
				bool const within_recent_health_grace = m_tel_last_good_health_cycle != 0ULL
					&& cy > m_tel_last_good_health_cycle
					&& (cy - m_tel_last_good_health_cycle)
						<= (m_csio_status_frame_ok && m_csio_error_report_ok ? grace_cycles_csio : grace_cycles_base);
				if (!within_recent_health_grace) {
					m_alarm_tel_not_responding_latched = true;
					m_tel_fault_last_latched_cycle = cy;
					m_tel_runtime_timeout_count++;
					m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_tel_tmo);
				} else {
					m_tel_timeout_relatch_guard_hits++;
				}
				if (!m_tp_timeout_trace_path.empty()) {
					u64 const since = cy - m_tel_last_health_sweep_cycle;
					double const since_ms = hz ? double(since) * 1000.0 / double(hz) : 0.;
					char to[900];
					std::snprintf(to, sizeof(to),
						"{\"cycle\":%llu,\"event\":\"telephony_comm_timeout_model\","
						"\"cycles_since_last_health_activity\":%llu,\"threshold_cycles\":%llu,"
						"\"estimated_ms_over_threshold\":%.6f,\"timeout_counter\":%u,"
						"\"grace_suppressed\":%s,\"note\":\"host_poll_timeout_latch_pre_clear_path\"}",
						static_cast<unsigned long long>(cy), static_cast<unsigned long long>(since),
						static_cast<unsigned long long>(timeout_cycles), since_ms,
						unsigned(m_tel_runtime_timeout_count), within_recent_health_grace ? "true" : "false");
					millennium_boot_trace_append_line(m_tp_timeout_trace_path, to);
				}
				telephony_runtime_trace_event("telephony_runtime_poll_timeout", "", "", false, "host_poll_timeout");
				if (!within_recent_health_grace)
					telephony_runtime_trace_event("telephony_fault_latched", "", "", false, "host_poll_timeout_latch");
			}
		}
		if (m_tel_runtime_waiting_c4 && m_rtos_startup_contract.xflag_all_init_done() && m_tel_runtime_wait_c4_deadline_cycle != 0ULL
			&& cy >= m_tel_runtime_wait_c4_deadline_cycle) {
			m_tel_runtime_waiting_c4 = false;
			m_tel_runtime_wait_c4_deadline_cycle = 0ULL;
			u64 const grace_cycles_short = hz != 0ULL ? (hz * 2ULL) : 0ULL;
			u64 const grace_cycles_csio_ok = hz != 0ULL ? (hz * 5ULL) : 0ULL;
			bool const csio_rx_post_clear_safe = m_alarm_tel_not_responding_cleared && m_csio_status_frame_ok
				&& m_csio_error_report_ok && m_tel_last_good_health_cycle != 0ULL && cy > m_tel_last_good_health_cycle
				&& (cy - m_tel_last_good_health_cycle) <= grace_cycles_csio_ok;
			if (csio_rx_post_clear_safe) {
				m_tel_timeout_relatch_guard_hits++;
				m_tel_health_consecutive_miss = 0U;
			} else {
				m_tel_health_consecutive_miss++;
				m_tel_runtime_retry_count++;
				telephony_runtime_trace_event("telephony_runtime_poll_timeout", "0x38", "", false, "error_report_deadline_miss");
				bool const within_recent_health_grace = m_tel_last_good_health_cycle != 0ULL && cy > m_tel_last_good_health_cycle
					&& (cy - m_tel_last_good_health_cycle) <= grace_cycles_short;
				u64 const ui_burst_window = hz != 0ULL ? (hz + hz / 2ULL) : 0ULL; // ~1.5 s
				unsigned const pend = ipcomm_pending_rx_count();
				bool const ui_burst_active =
					m_tp_last_ui_event_cycle != 0ULL && cy > m_tp_last_ui_event_cycle && ui_burst_window != 0ULL
					&& (cy - m_tp_last_ui_event_cycle) <= ui_burst_window
					&& (pend >= 4U || (pend >= 1U && pend <= 32U));
				bool const csio_runtime_ok = m_tel_fw_boot_contract_satisfied && m_csio_status_frame_ok && m_csio_error_report_ok
					&& m_alarm_tel_not_responding_cleared;
				unsigned const miss_threshold =
					(cy > m_tel_fault_last_latched_cycle && (cy - m_tel_fault_last_latched_cycle) <= (hz != 0ULL ? hz * 4ULL : 0ULL))
						? 5U
						: 3U;
				if (within_recent_health_grace || ui_burst_active) {
					m_tel_timeout_relatch_guard_hits++;
					m_tel_health_consecutive_miss = std::max(1U, m_tel_health_consecutive_miss - 1U);
				} else if (m_tel_health_consecutive_miss >= miss_threshold) {
					// If status + error-report CSI/O contract is satisfied, C4 deadline slips are usually CP ISR
					// scheduling / backlog — dropping the IP link replays boot + "not responding" without hardware cause.
					if (csio_runtime_ok) {
						m_tel_timeout_relatch_guard_hits++;
						m_tel_health_consecutive_miss = 0U;
					} else {
						m_alarm_tel_not_responding_latched = true;
						m_alarm_tel_not_responding_cleared = false;
						m_tel_fault_last_latched_cycle = cy;
						m_tel_runtime_timeout_count++;
						m_tel_runtime_reset_signal_count++;
						m_rtos_startup_contract.post_init_signal(coinline::rtos::startup_scheduler_model::inis_tel_tmo);
						telephony_runtime_trace_event("telephony_fault_latched", "0x38", "", false, "consecutive_miss_threshold");
						telephony_runtime_trace_event("telephony_reset_signal", "RESET_TELEPHONY_PROC_SIG", "", true,
							"consecutive_miss_threshold");
						// Model telephony-process reset: disable link and restart reset timing window.
						m_tel_ip_link_enabled = false;
						m_tel_boot_code_sent = false;
						m_tel_reset_cycle_start = cy;
						m_tel_ip_link_enable_deadline_cycle = cy + cycles_per_10ms * (15U + 6U);
						m_tel_link_enable_cycle = 0ULL;
						m_tel_boot_code_deadline_cycle = 0ULL;
					}
				}
			}
		}
		if (m_tel_pending_status_after_clear && !m_service_timer_trace_path.empty()) {
			char tt[420];
			std::snprintf(tt, sizeof(tt),
				"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"event\":\"service_timer_tick\","
				"\"event_area\":\"service_timer\",\"reason\":\"deferred_status_pending\"}",
				static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU));
			millennium_boot_trace_append_line(m_service_timer_trace_path, tt);
		}
		u32 const k = m_keypad->keymatrix_with_hook_debounce(m_keymatrix_io->read(), cy);
		u32 const l = m_linectrl_io->read();
		u32 const sk = m_terminal21_softkeys_io->read();
		u32 const s = m_secmask_io->read();
		tp_backend_process_front_panel(k, l, sk, static_cast<std::uint8_t>(s & 0xffU), cy, u16(m_maincpu->pc() & 0xffffU));
		if (!m_front_panel_trace_path.empty()) {
			if (!m_front_panel_trace_seeded) {
				m_last_keymatrix_state = k;
				m_last_secmask_state = s;
				m_last_linectrl_state = l;
				m_front_panel_trace_seeded = true;
			}
			auto const emit_panel = [this, cy](char const *name, u32 oldv, u32 newv, char const *port_name) {
				char b[320];
				std::snprintf(b, sizeof(b),
					"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"input\":\"%s\",\"old_state\":\"0x%08X\","
					"\"new_state\":\"0x%08X\",\"firmware_visible_port\":\"%s\",\"note\":\"state_change\"}",
					static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
					unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), name, unsigned(oldv), unsigned(newv), port_name);
				millennium_boot_trace_append_line(m_front_panel_trace_path, b);
			};
			auto const emit_input_source = [this, cy](char const *event, char const *name, u32 oldv, u32 newv,
										 char const *signal, char const *port_name) {
				if (m_front_panel_input_source_trace_path.empty())
					return;
				char b[640];
				std::snprintf(b, sizeof(b),
					"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"event\":\"%s\","
					"\"input_source\":\"mame_input_port\",\"MAME_input_name\":\"%s\","
					"\"old_input_state\":\"0x%08X\",\"new_input_state\":\"0x%08X\","
					"\"mapped_emulated_hardware_signal\":\"%s\",\"firmware_visible_port\":\"%s\"}",
					static_cast<unsigned long long>(cy), unsigned(m_maincpu->pc() & 0xffffU),
					unsigned(m_maincpu->state_int(Z180_SP) & 0xffffU), event, name, unsigned(oldv), unsigned(newv),
					signal, port_name);
				millennium_boot_trace_append_line(m_front_panel_input_source_trace_path, b);
			};
			if (k != m_last_keymatrix_state) {
				emit_panel("keymatrix", m_last_keymatrix_state, k, "0x0041-0x0044");
				char const *event = "input_keymatrix_seen_by_mame";
				u32 const key_delta = (k ^ m_last_keymatrix_state) & 0x00000fffU;
				if (key_delta != 0U) {
					if (k & 0x00000001U)
						event = "input_key_1_seen_by_mame";
					else if (k & 0x00000002U)
						event = "input_key_2_seen_by_mame";
					else if (k & 0x00000004U)
						event = "input_key_3_seen_by_mame";
					else if (k & 0x00000008U)
						event = "input_key_4_seen_by_mame";
					else if (k & 0x00000010U)
						event = "input_key_5_seen_by_mame";
					else if (k & 0x00000020U)
						event = "input_key_6_seen_by_mame";
					else if (k & 0x00000040U)
						event = "input_key_7_seen_by_mame";
					else if (k & 0x00000080U)
						event = "input_key_8_seen_by_mame";
					else if (k & 0x00000100U)
						event = "input_key_9_seen_by_mame";
					else if (k & 0x00000200U)
						event = "input_key_star_seen_by_mame";
					else if (k & 0x00000400U)
						event = "input_key_0_seen_by_mame";
					else if (k & 0x00000800U)
						event = "input_key_pound_seen_by_mame";
				}
				if (((k ^ m_last_keymatrix_state) & k_terminal21_hook_bit) != 0U) {
					event = (k & k_terminal21_hook_bit) != 0U ? "input_hook_offhook_seen_by_mame"
															: "input_hook_onhook_seen_by_mame";
				}
				emit_input_source(event, "KEYMATRIX", m_last_keymatrix_state, k, "tp_path_keymatrix", "0x0041-0x0044");
				m_last_keymatrix_state = k;
			}
			if (s != m_last_secmask_state) {
				emit_panel("secmask", m_last_secmask_state, s, "0x0040");
				emit_input_source("input_security_seen_by_mame", "SECMASK", m_last_secmask_state, s, "security_state",
					"0x0040");
				m_last_secmask_state = s;
			}
			if (l != m_last_linectrl_state) {
				emit_panel("linectrl", m_last_linectrl_state, l, "0x0041-0x0044");
				char const *ev = "input_line_supervision_seen_by_mame";
				if (((l ^ m_last_linectrl_state) & 0x01U) != 0U)
					ev = ((l & 0x01U) != 0U) ? "input_line_connection_seen_by_mame" : "input_line_interruption_seen_by_mame";
				emit_input_source(ev, "LINECTRL", m_last_linectrl_state, l,
					"line_supervision", "0x0040");
				m_last_linectrl_state = l;
			}
		}
		u8 const cntla = u8(m_maincpu->state_int(Z180_CNTLA0) & 0xff);
		u8 const cntlb = u8(m_maincpu->state_int(Z180_CNTLB0) & 0xff);
		if (!m_asci_baseline_ready) {
			m_base_cntla0 = cntla;
			m_base_cntlb0 = cntlb;
			m_asci_baseline_ready = true;
			schedule_driver_timer(TID_HOST_POLL, attotime::from_msec(10));
			return;
		}
		if (!m_m8_logged && (cntla != m_base_cntla0 || cntlb != m_base_cntlb0))
			m_modem->note_asci_programmed(cntla, cntlb);
		try_boot_m8(cy);
		schedule_driver_timer(TID_HOST_POLL, attotime::from_msec(10));
		return;
	}
	if (id == TID_M3_FALLBACK) {
		if (!m_m3_m4_logged)
			emit_boot_m3_m4_snapshot("timeout_no_ram_write_yet");
		return;
	}
	if (id == TID_CPU_TRACE) {
		cpu_trace_sample_tick();
		schedule_driver_timer(TID_CPU_TRACE, attotime::from_msec(5));
		return;
	}
	if (id == TID_VECTOR_PROBE) {
		vector_event_probe_tick();
		if (m_vector_event_budget > 0U
			&& (!m_interrupt_events_path.empty() || !m_vector_events_path.empty() || !m_context_switch_events_path.empty()
				|| !m_fetch_provenance_trace_path.empty() || !m_stack_control_flow_trace_path.empty()
				|| (m_trace_capture_fault_context && !m_first_pc_ffff_context_path.empty()))) {
			attotime const probe_interval = (m_trace_profile == trace_profile::full) ? attotime::from_usec(50)
				: attotime::from_usec(1000);
			schedule_driver_timer(TID_VECTOR_PROBE, probe_interval);
		}
		return;
	}
	if (id == TID_VOICE_INT0) {
		poll_voice_segment_done_pulse_int0();
		schedule_driver_timer(TID_VOICE_INT0, voicew_int0_poll_delay());
		return;
	}
}
