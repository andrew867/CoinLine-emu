// SPDX-License-Identifier: GPL-2.0-or-later
// Map a VFD cell byte to UTF-8 for the OEM font row (host prompt macros + panel CGROM layout).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace millennium_vfd_cell_utf8 {

inline void append_codepoint(std::string &out, std::uint32_t cp)
{
	if (cp <= 0x7fU) {
		out.push_back(static_cast<char>(cp));
	} else if (cp <= 0x7ffU) {
		out.push_back(static_cast<char>(0xc0U | ((cp >> 6) & 0x1fU)));
		out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
	} else if (cp <= 0xffffU) {
		out.push_back(static_cast<char>(0xe0U | ((cp >> 12) & 0x0fU)));
		out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3fU)));
		out.push_back(static_cast<char>(0x80U | (cp & 0x3fU)));
	}
}

/// Append one logical character for `b` given host katakana-font selection (separate from Latin prompt mapping).
inline void append_cell(std::string &out, std::uint8_t b, bool katakana_mode)
{
	if (b < 0x20U) {
		out.push_back('.');
		return;
	}
	if (b <= 0x7eU) {
		out.push_back(static_cast<char>(b));
		return;
	}
	if (b == 0x7fU) {
		append_codepoint(out, 0x2302U); // ⌂ — distinct from printable ASCII; VFD DEL glyph
		return;
	}
	// CGROM 0x80–0x9F are blank in the reference glyph table; keep the row visually empty.
	if (b <= 0x9fU) {
		out.push_back(' ');
		return;
	}
	if (b == 0xa0U) {
		append_codepoint(out, 0x00a0U); // NBSP (Latin-1 A0; VFD has a sparse glyph)
		return;
	}
	if (katakana_mode && b >= 0xa1U && b <= 0xdfU) {
		append_codepoint(out, 0xFF61U + std::uint32_t(b - 0xa1U));
		return;
	}
	// European accent / symbol prompt codes 0xA1–0xB7 (only when not using HW katakana for A1–DF).
	if (b >= 0xa1U && b <= 0xb7U) {
		static constexpr std::uint32_t k_latin[] = {
			0x00e0U, // A1 AGV à
			0x00e1U, // A2 AAC á
			0x00e2U, // A3 ACX â
			0x00e4U, // A4 AUM ä
			0x00e8U, // A5 EGV è
			0x00e9U, // A6 EAC é
			0x00eaU, // A7 ECX ê
			0x00ebU, // A8 EUM ë
			0x00edU, // A9 IAC í
			0x00eeU, // AA ICX î
			0x00efU, // AB IUM ï
			0x00e7U, // AC CCD ç
			0x00f9U, // AD UGV ù
			0x00faU, // AE UAC ú
			0x00fbU, // AF UCX û
			0x00fcU, // B0 UUM ü
			0x00f3U, // B1 OAC ó
			0x00f4U, // B2 OCX ô
			0x00f6U, // B3 OUM ö
			0x00f1U, // B4 NQQ ñ
			0x00b8U, // B5 CID ¸
			0x00a2U, // B6 CNT ¢
			0x25c6U, // B7 NXC ◆
		};
		append_codepoint(out, k_latin[std::size_t(b - 0xa1U)]);
		return;
	}
	if (b == 0xb8U) {
		append_codepoint(out, 0x2588U); // █ BCK / ALP
		return;
	}
	if (b == 0xb9U) {
		append_codepoint(out, 0x25bcU); // ▼ (ROM “active cursor” glyph; may double as locale currency)
		return;
	}
	if (b == 0xbaU) {
		append_codepoint(out, 0x2016U); // ‖
		return;
	}
	if (b <= 0xbfU || b == 0xc0U) {
		out.push_back(' ');
		return;
	}
	if (b <= 0xfeU) {
		// Japanese extension codes 0xC1–0xFE: sequential Katakana block approximation.
		append_codepoint(out, 0x30a1U + std::uint32_t(b - 0xc1U));
		return;
	}
	// 0xFF: host uses this code for superscript-o style glyph in prompts.
	append_codepoint(out, 0x00baU); // º
}

} // namespace millennium_vfd_cell_utf8
