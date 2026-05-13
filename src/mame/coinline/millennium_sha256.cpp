// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compact SHA-256 (FIPS 180-4) for firmware verification. No external deps.

#include "millennium_sha256.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace {

constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) { return (x >> n) | (x << (32U - n)); }

constexpr std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (~x & z); }
constexpr std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
constexpr std::uint32_t big_sigma0(std::uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
constexpr std::uint32_t big_sigma1(std::uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
constexpr std::uint32_t small_sigma0(std::uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
constexpr std::uint32_t small_sigma1(std::uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

std::array<std::uint32_t, 8> g_h{};
std::uint64_t g_total_len{};
std::array<std::uint8_t, 64> g_buf{};
unsigned g_buflen{};

void transform(std::uint8_t const block[64])
{
	static constexpr std::uint32_t k[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
	};

	std::uint32_t w[64];
	for (int i = 0; i < 16; ++i) {
		w[i] = (std::uint32_t(block[i * 4 + 0]) << 24) | (std::uint32_t(block[i * 4 + 1]) << 16)
			| (std::uint32_t(block[i * 4 + 2]) << 8) | std::uint32_t(block[i * 4 + 3]);
	}
	for (int i = 16; i < 64; ++i)
		w[i] = small_sigma1(w[i - 2]) + w[i - 7] + small_sigma0(w[i - 15]) + w[i - 16];

	std::uint32_t a = g_h[0], b = g_h[1], c = g_h[2], d = g_h[3];
	std::uint32_t e = g_h[4], f = g_h[5], g = g_h[6], h = g_h[7];
	for (int i = 0; i < 64; ++i) {
		std::uint32_t const t1 = h + big_sigma1(e) + ch(e, f, g) + k[i] + w[i];
		std::uint32_t const t2 = big_sigma0(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	g_h[0] += a;
	g_h[1] += b;
	g_h[2] += c;
	g_h[3] += d;
	g_h[4] += e;
	g_h[5] += f;
	g_h[6] += g;
	g_h[7] += h;
}

} // namespace

void millennium_sha256_reset()
{
	g_h = { 0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U };
	g_total_len = 0;
	g_buflen = 0;
	std::memset(g_buf.data(), 0, g_buf.size());
}

void millennium_sha256_update(std::uint8_t const *data, std::size_t len)
{
	g_total_len += len;
	while (len > 0) {
		unsigned const take = std::min<unsigned>(64U - g_buflen, unsigned(len));
		std::memcpy(g_buf.data() + g_buflen, data, take);
		g_buflen += take;
		data += take;
		len -= take;
		if (g_buflen == 64) {
			transform(g_buf.data());
			g_buflen = 0;
		}
	}
}

void millennium_sha256_finish(std::uint8_t out_digest[32])
{
	std::uint64_t const bitlen = g_total_len * 8U;
	unsigned i = g_buflen;
	g_buf[i++] = 0x80;
	if (i > 56) {
		while (i < 64)
			g_buf[i++] = 0;
		transform(g_buf.data());
		i = 0;
	}
	while (i < 56)
		g_buf[i++] = 0;
	for (unsigned j = 0; j < 8; ++j)
		g_buf[56 + j] = std::uint8_t((bitlen >> (56U - 8U * j)) & 0xffU);
	transform(g_buf.data());

	for (int w = 0; w < 8; ++w) {
		out_digest[w * 4 + 0] = std::uint8_t((g_h[w] >> 24) & 0xffU);
		out_digest[w * 4 + 1] = std::uint8_t((g_h[w] >> 16) & 0xffU);
		out_digest[w * 4 + 2] = std::uint8_t((g_h[w] >> 8) & 0xffU);
		out_digest[w * 4 + 3] = std::uint8_t(g_h[w] & 0xffU);
	}
}

std::string millennium_sha256_hex(std::uint8_t const *data, std::size_t len)
{
	millennium_sha256_reset();
	millennium_sha256_update(data, len);
	std::uint8_t d[32];
	millennium_sha256_finish(d);
	std::ostringstream os;
	os << std::hex << std::setfill('0');
	for (unsigned b = 0; b < 32; ++b)
		os << std::setw(2) << int(d[b]);
	return os.str();
}
