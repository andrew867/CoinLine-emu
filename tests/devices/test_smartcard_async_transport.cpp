// SPDX-License-Identifier: GPL-2.0-or-later
#include "millennium_smartcard_model.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {
using bytes = std::vector<std::uint8_t>;

void pins(millennium_smartcard_model &card, unsigned h, unsigned d, unsigned e, unsigned f, std::uint64_t at)
{
	card.update_later_pins(d, e, f, at);
	card.update_async_later_pins(h, d, e, at);
}

void activate(millennium_smartcard_model &card, bool internal = false)
{
	card.insert_card_at(0, 1000000);
	pins(card, 0, 3, 8, 0, 10);
	pins(card, 0, 3, 8, 2, 20);
	pins(card, 0, 3, 8, 0, 30);
	pins(card, 0, 3, 9, 0, 10000);
	if (!internal)
		pins(card, 0, 3, 13, 0, 20000);
}

bytes read_all(millennium_smartcard_model &card, std::uint64_t at)
{
	bytes result;
	while (auto next = card.async_read_byte(at))
		result.push_back(*next);
	return result;
}

void write(millennium_smartcard_model &card, bytes const &data, std::uint64_t at)
{
	for (auto byte : data)
		card.async_write_byte(byte, at);
}

bytes block(unsigned pcb, bytes data)
{
	bytes result{0, std::uint8_t(pcb), std::uint8_t(data.size())};
	result.insert(result.end(), data.begin(), data.end());
	std::uint8_t lrc = 0;
	for (auto byte : result)
		lrc ^= byte;
	result.push_back(lrc);
	return result;
}

int check_t0()
{
	millennium_smartcard_model card;
	std::string error;
	if (!card.parse_fixture_json(R"({"protocol":"async","application":"lab-readonly","reset_policy":"external","atr_hex":"3B 00","memory_hex":"54455354","atr_delay_us":4000})", error))
		return 1;
	card.insert_card_at(0, 1000000);
	pins(card, 8, 3, 9, 0, 10);
	if (card.async_routed() || card.async_read_byte(10000)) return 2;
	pins(card, 0, 3, 8, 0, 20); // supply request alone is insufficient
	pins(card, 0, 3, 9, 0, 10000);
	if (card.async_routed() || card.async_read_byte(20000)) return 3;
	pins(card, 0, 3, 9, 2, 20000);
	if (!card.async_routed() || card.async_active() || card.async_read_byte(25000)) return 4;
	pins(card, 0, 3, 13, 0, 30000);
	if (!card.async_active() || card.async_read_byte(33999) || read_all(card, 34000) != bytes{0x3b, 0}) return 5;
	write(card, {0, 0xb0, 0, 0, 4}, 35000);
	if (read_all(card, 36000) != bytes{0x69, 0x85}) return 6;
	write(card, {0, 0xa4, 4, 0, 5}, 37000);
	if (read_all(card, 37999).size() || read_all(card, 38000) != bytes{0xa4}) return 7;
	write(card, {0xf0, 0x4c, 0x41, 0x42, 1}, 39000);
	if (read_all(card, 40000) != bytes{0x90, 0}) return 8;
	write(card, {0, 0xb0, 0, 0, 4}, 41000);
	if (read_all(card, 42000) != bytes{0xb0, 'T', 'E', 'S', 'T', 0x90, 0}) return 9;
	write(card, {0, 0xb0, 0, 3, 2}, 43000);
	if (read_all(card, 44000) != bytes{0x6b, 0}) return 10;
	write(card, {0, 0xd6, 0, 0, 4}, 45000);
	if (read_all(card, 46000) != bytes{0x6d, 0} || card.memory() != bytes{'T','E','S','T'}) return 11;
	write(card, {0, 0xb0, 0, 0, 4}, 47000);
	pins(card, 0, 1, 13, 0, 47500); // select an unpopulated module slot
	if (card.async_routed() || card.async_read_byte(50000)) return 12;
	pins(card, 0, 3, 13, 0, 50000); // no new reset, no stale bytes
	if (card.async_active() || card.async_read_byte(60000)) return 13;
	pins(card, 0, 3, 9, 0, 61000);
	pins(card, 0, 3, 13, 0, 62000);
	if (read_all(card, 66000) != bytes{0x3b, 0}) return 14;
	write(card, {0, 0xb0, 0, 0, 4}, 67000);
	if (read_all(card, 68000) != bytes{0x69, 0x85}) return 15;
	write(card, {0, 0xa4, 0, 0, 2}, 69000);
	if (read_all(card, 70000) != bytes{0xa4}) return 17;
	write(card, {0x70, 1}, 71000);
	if (read_all(card, 72000) != bytes{0x90, 0}) return 18;
	write(card, {0, 0xb0, 0, 0, 4}, 73000);
	if (read_all(card, 74000) != bytes{0xb0,'T','E','S','T',0x90,0}) return 19;
	card.remove_card();
	if (card.async_routed() || card.async_read_byte(70000)) return 16;
	return 0;
}

int check_t1()
{
	millennium_smartcard_model card;
	std::string error;
	if (!card.parse_fixture_json(R"({"protocol":"async","application":"lab-readonly","reset_policy":"internal","atr_hex":"3B 80 01 81","memory_hex":"54455354","atr_delay_us":4000})", error)) return 20;
	activate(card, true);
	if (!card.async_active() || card.async_read_byte(13999) || read_all(card, 14000) != bytes{0x3b, 0x80, 1, 0x81}) return 21;
	auto bad = block(0, {0, 0xb0, 0, 0, 4});
	bad.back() ^= 1;
	write(card, bad, 15000);
	if (read_all(card, 16000) != block(0x81, {})) return 22;
	auto first = block(0x20, {0, 0xa4, 4, 0, 5});
	write(card, first, 17000);
	if (read_all(card, 18000) != block(0x90, {})) return 23;
	write(card, first, 19000);
	if (read_all(card, 20000) != block(0x90, {})) return 24;
	write(card, block(0x40, {0xf0, 0x4c, 0x41, 0x42, 1}), 21000);
	if (read_all(card, 22000) != block(0, {0x90, 0})) return 25;
	auto request = block(0, {0, 0xb0, 0, 0, 4});
	write(card, request, 23000);
	auto const reply = block(0x40, {'T','E','S','T',0x90,0});
	if (read_all(card, 24000) != reply) return 26;
	write(card, request, 25000);
	if (read_all(card, 26000) != reply) return 27;
	write(card, block(0x91, {}), 27000);
	if (read_all(card, 28000) != reply) return 28;
	write(card, block(0xc0, {}), 29000);
	if (read_all(card, 30000) != block(0xe0, {})) return 29;
	write(card, request, 31000);
	if (read_all(card, 32000) != block(0, {'T','E','S','T',0x90,0})) return 30;
	write(card, block(0xc0, {}), 33000);
	if (read_all(card, 34000) != block(0xe0, {})) return 34;
	write(card, block(0xc1, {3}), 35000);
	if (read_all(card, 36000) != block(0xe1, {3})) return 35;
	write(card, request, 37000);
	if (read_all(card, 38000) != block(0x20, {'T','E','S'})) return 36;
	write(card, block(0x91, {}), 39000);
	if (read_all(card, 40000) != block(0x20, {'T','E','S'})) return 37;
	write(card, block(0x90, {}), 41000);
	if (read_all(card, 42000) != block(0x40, {'T',0x90,0})) return 38;
	card.remove_card();
	if (!card.parse_fixture_json(R"({"protocol":"async","atr_hex":"3B 80 01 80","atr_delay_us":4000})", error)) return 31;
	activate(card);
	if (read_all(card, 24000) != bytes{0x3b,0x80,1,0x80} || card.selected_protocol() != millennium_smartcard_model::negotiated_protocol::none) return 32;
	write(card, request, 25000);
	if (!read_all(card, 26000).empty()) return 33;
	return 0;
}

int check_pps()
{
	millennium_smartcard_model card;
	std::string error;
	if (!card.parse_fixture_json(R"({"protocol":"async","application":"lab-readonly","atr_hex":"3B 10 12","memory_hex":"54455354"})", error)) return 40;
	activate(card);
	if (read_all(card, 24000) != bytes{0x3b, 0x10, 0x12}) return 41;
	write(card, {0xff, 0x10, 0x12, 0xfc}, 25000);
	if (!read_all(card, 26000).empty() || card.async_take_parameters()) return 42;
	pins(card, 0, 3, 9, 0, 27000);
	pins(card, 0, 3, 13, 0, 28000);
	if (read_all(card, 32000) != bytes{0x3b, 0x10, 0x12}) return 43;
	write(card, {0xff, 0x10, 0x12, 0xfd}, 33000);
	if (card.async_take_parameters() || read_all(card, 34000) != bytes{0xff,0x10,0x12,0xfd}) return 44;
	auto const pps = card.async_take_parameters();
	if (!pps || pps->fi != 372 || pps->di != 2 || card.async_take_parameters()) return 45;
	return 0;
}
}

int main()
{
	if (int const result = check_t0()) { std::cerr << "T=0 failure " << result << '\n'; return result; }
	if (int const result = check_t1()) { std::cerr << "T=1 failure " << result << '\n'; return result; }
	if (int const result = check_pps()) { std::cerr << "PPS failure " << result << '\n'; return result; }
	std::cout << "smartcard_async_transport ok\n";
	return 0;
}
