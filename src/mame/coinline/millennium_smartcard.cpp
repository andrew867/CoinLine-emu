// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_smartcard.h"

#include "millennium_firmware.h"
#include "millennium_sha256.h"

#include <array>

DEFINE_DEVICE_TYPE(MILLENNIUM_SMARTCARD, millennium_smartcard_device, "millennium_smartcard",
	"CoinLine ISO7816 smart card")

millennium_smartcard_device::millennium_smartcard_device(machine_config const &mconfig, char const *tag,
	device_t *owner, u32 clock)
	: device_t(mconfig, MILLENNIUM_SMARTCARD, tag, owner, clock)
	, device_nvram_interface(mconfig, *this)
	, device_serial_interface(mconfig, *this)
	, m_txd(*this)
{
}

void millennium_smartcard_device::device_start()
{
	m_async_timer = timer_alloc(FUNC(millennium_smartcard_device::async_serial_tick), this);
	char const *trace = osd_getenv("COINLINE_SMART_CARD_TRACE");
	m_pin_trace = trace && *trace == '1';
	char const *lab_trace = osd_getenv("COINLINE_SMART_CARD_LAB_TRACE");
	m_lab_serial_trace = lab_trace && *lab_trace == '1';
	std::string err;
	char const *p = osd_getenv("COINLINE_SMART_CARD");
	std::string const rel =
		(p && *p) ? std::string(p) : std::string("fixtures/cards/smartcard-sync-test.json");
	std::vector<std::uint8_t> raw;
	std::string path = rel;
	if (!millennium_read_file(path, raw, err)) {
		char const *root = osd_getenv("COINLINE_EMU_ROOT");
		if (root && *root) {
			std::string r(root);
			while (!r.empty() && (r.back() == '/' || r.back() == '\\'))
				r.pop_back();
			path = r + "/" + rel;
			raw.clear();
			millennium_read_file(path, raw, err);
		}
	}
	if (!raw.empty()) {
		std::string const txt(reinterpret_cast<char const *>(raw.data()), raw.size());
		if (!m_model.parse_fixture_json(txt, err))
			osd_printf_warning("millennium_smartcard: fixture parse failed: %s\n", err.c_str());
		else {
			m_fixture_path = path;
			m_fixture_bytes = std::move(raw);
			m_fixture_digest = millennium_sha256_hex(m_fixture_bytes.data(), m_fixture_bytes.size());
		}
	}
}

void millennium_smartcard_device::device_reset()
{
	device_t::device_reset();
	m_model.remove_card();
	stop_async_serial();
}

bool millennium_smartcard_device::reload_fixture_from_path(std::string const &path, std::string &error_out)
{
	std::vector<std::uint8_t> raw;
	if (!millennium_read_file(path, raw, error_out))
		return false;
	// Reinserting the same virtual card must not replenish its test memory.
	// Loading a different fixture creates a distinct, explicitly selected card.
	if (path == m_fixture_path && raw == m_fixture_bytes)
		return true;
	std::string const txt(reinterpret_cast<char const *>(raw.data()), raw.size());
	millennium_smartcard_model candidate;
	if (!candidate.parse_fixture_json(txt, error_out))
		return false;
	m_model = std::move(candidate);
	m_fixture_path = path;
	m_fixture_bytes = std::move(raw);
	m_fixture_digest = millennium_sha256_hex(m_fixture_bytes.data(), m_fixture_bytes.size());
	return true;
}

void millennium_smartcard_device::nvram_default()
{
	// The immutable fixture remains the initial contents. No card contents
	// are synthesized from the terminal battery or payment ledger.
}

bool millennium_smartcard_device::nvram_read(util::read_stream &file)
{
	if (!nvram_can_write())
		return false;
	std::array<char, 64> digest{};
	auto const [header_error, header_size] = util::read(file, digest.data(), digest.size());
	if (header_error || header_size != digest.size() || std::string(digest.data(), digest.size()) != m_fixture_digest)
		return false;
	std::vector<std::uint8_t> memory(m_model.memory().size());
	auto const [error, actual] = util::read(file, memory.data(), memory.size());
	if (error || actual != memory.size() || !m_model.restore_sync_test_memory(memory))
		return false;
	if (m_pin_trace)
		osd_printf_info("smart-card synthetic memory restored\n");
	return true;
}

bool millennium_smartcard_device::nvram_write(util::write_stream &file)
{
	if (!nvram_can_write())
		return false;
	auto const [header_error, header_size] = util::write(file, m_fixture_digest.data(), m_fixture_digest.size());
	if (header_error || header_size != m_fixture_digest.size())
		return false;
	auto const &memory = m_model.memory();
	auto const [error, actual] = util::write(file, memory.data(), memory.size());
	return !error && actual == memory.size();
}

void millennium_smartcard_device::insert_card(u64 cycle, u64 cpu_hz)
{
	if (m_serial_model == &m_model)
		stop_async_serial();
	m_async_hz = cpu_hz ? cpu_hz : 12288000;
	m_model.insert_card_at(cycle, cpu_hz);
	if (m_pin_trace)
		osd_printf_info("smart-card physical insert at %llu present=%u\n",
			static_cast<unsigned long long>(async_cycle()), unsigned(m_model.status_lines() & 1U));
}

void millennium_smartcard_device::remove_card()
{
	m_model.remove_card();
	if (m_serial_model == &m_model)
		stop_async_serial();
	if (m_pin_trace)
		osd_printf_info("smart-card physical remove at %llu present=%u\n",
			static_cast<unsigned long long>(async_cycle()), unsigned(m_model.status_lines() & 1U));
}

std::uint8_t millennium_smartcard_device::read_fifo(u64 cycle)
{
	return m_model.read_fifo(cycle);
}

void millennium_smartcard_device::write_command(u8 data, u64 cycle)
{
	m_model.write_command(data, cycle);
}

std::uint8_t millennium_smartcard_device::status_lines() const
{
	return m_model.status_lines();
}

void millennium_smartcard_device::notify_reset(u64 cycle)
{
	m_model.notify_reset(cycle);
}

void millennium_smartcard_device::later_pins(u8 d, u8 e, u8 f, u64 cycle)
{
	m_model.update_later_pins(d, e, f, cycle);
	if (m_pin_trace && (m_model.status_lines() & 1U))
		osd_printf_info("smart-card pins %llu D=%02X E=%02X F=%02X power=%u bit=%zu\n",
			static_cast<unsigned long long>(cycle), unsigned(d), unsigned(e), unsigned(f),
			unsigned(m_model.sync_powered()), m_model.sync_bit_address());
}

u64 millennium_smartcard_device::async_cycle() const
{
	return machine().time().as_ticks(m_async_hz);
}

u8 millennium_smartcard_device::inverse_octet(u8 value)
{
	value = ~value;
	u8 reversed = 0;
	for (unsigned bit = 0; bit < 8U; ++bit)
		reversed = u8((reversed << 1U) | ((value >> bit) & 1U));
	return reversed;
}

void millennium_smartcard_device::stop_async_serial()
{
	m_async_serial = false;
	m_serial_model = nullptr;
	m_async_timer->adjust(attotime::never);
	set_rate(attotime::never);
	receive_register_reset();
	transmit_register_reset();
	m_async_guard_until = attotime::zero;
	m_async_period = attotime::never;
	m_txd(1);
}

void millennium_smartcard_device::later_async_pins(u8 h, u8 d, u8 e, millennium_smartcard_model *internal_contact)
{
	m_model.update_async_later_pins(h, d, e, async_cycle());
	auto *next = m_model.async_active() ? &m_model : internal_contact;
	if (next == m_serial_model)
		return;
	if (m_async_serial)
		stop_async_serial();
	if (next) {
		// Default ISO 7816 ETU is 372 card-clock periods. A minimal ATR
		// does not negotiate a different F/D value. Do not copy a wrong
		// terminal baud setting into the independent card clock.
		m_async_serial = true;
		m_serial_model = next;
		set_data_frame(1, 8, next->async_inverse() ? PARITY_ODD : PARITY_EVEN, STOP_BITS_1);
		m_async_period = attotime::from_ticks(372, 3579540);
		set_rate(m_async_period);
		m_txd(1);
		if (m_pin_trace)
			osd_printf_info("smart-card serial activation at %llu\n", static_cast<unsigned long long>(async_cycle()));
		schedule_async_serial();
	}
}

void millennium_smartcard_device::schedule_async_serial()
{
	if (!m_async_serial || !m_serial_model || !m_serial_model->async_active() || !is_transmit_register_empty() || is_receive_register_shifting())
		return;
	if (auto const ready = m_serial_model->async_next_ready_cycle()) {
		u64 const cycle = async_cycle();
		attotime const now = machine().time();
		attotime delay = *ready > cycle ? attotime::from_ticks(*ready - cycle, m_async_hz) : attotime::zero;
		if (m_async_guard_until > now && m_async_guard_until - now > delay)
			delay = m_async_guard_until - now;
		m_async_timer->adjust(delay);
	}
}

TIMER_CALLBACK_MEMBER(millennium_smartcard_device::async_serial_tick)
{
	if (!m_async_serial || !m_serial_model || !m_serial_model->async_active() || !is_transmit_register_empty() || is_receive_register_shifting())
		return;
	if (machine().time() < m_async_guard_until) {
		schedule_async_serial();
		return;
	}
	if (auto const byte = m_serial_model->async_read_byte(async_cycle())) {
		transmit_register_setup(m_serial_model->async_inverse() ? inverse_octet(*byte) : *byte);
		if (m_lab_serial_trace && m_serial_model->async_lab_application())
			osd_printf_info("synthetic card TX %02X\n", unsigned(*byte));
		if (m_pin_trace)
			osd_printf_info("smart-card serial character transmitted at %llu\n", static_cast<unsigned long long>(async_cycle()));
	} else
		schedule_async_serial();
}

void millennium_smartcard_device::rxd_w(int state)
{
	if (m_async_serial && is_transmit_register_empty())
		rx_w(state);
}

void millennium_smartcard_device::tra_callback()
{
	if (m_async_serial)
		m_txd(transmit_register_get_data_bit());
}

void millennium_smartcard_device::tra_complete()
{
	if (!m_async_serial || !m_serial_model)
		return;
	// The previous character's stop bit is still on the wire. Leave at
	// least two additional ETUs before the next card start bit.
	m_async_guard_until = machine().time() + m_async_period * 3;
	// A lab fixture may explicitly declare additional device processing time
	// between characters; it is not inferred from the terminal's baud rate.
	m_async_guard_until += attotime::from_usec(m_serial_model->async_inter_character_delay_us());
	if (auto const parameters = m_serial_model->async_take_parameters()) {
		// PPS is echoed at the initial rate; change the serial clock only
		// after the last response character has been transmitted.
		m_async_period = attotime::from_ticks(parameters->fi, 3579540U * parameters->di);
		set_rate(m_async_period);
	}
	schedule_async_serial();
}

void millennium_smartcard_device::rcv_complete()
{
	receive_register_extract();
	if (!m_async_serial || !m_serial_model || !is_transmit_register_empty() || is_receive_framing_error() || is_receive_parity_error())
		return;
	u8 const byte = get_received_char();
	if (m_lab_serial_trace && m_serial_model->async_lab_application())
		osd_printf_info("synthetic card RX %02X\n", unsigned(m_serial_model->async_inverse() ? inverse_octet(byte) : byte));
	m_serial_model->async_write_byte(m_serial_model->async_inverse() ? inverse_octet(byte) : byte, async_cycle());
	schedule_async_serial();
	if (m_pin_trace)
		osd_printf_info("smart-card serial character received at %llu\n", static_cast<unsigned long long>(async_cycle()));
}

void millennium_smartcard_device::earlier_pins(u8 a, u8 c, u64 cycle)
{
	m_model.update_earlier_pins(a, c, cycle);
	if (m_pin_trace && (m_model.status_lines() & 1U))
		osd_printf_info("smart-card pins %llu A=%02X C=%02X power=%u bit=%zu\n",
			static_cast<unsigned long long>(cycle), unsigned(a), unsigned(c),
			unsigned(m_model.sync_powered()), m_model.sync_bit_address());
}

u8 millennium_smartcard_device::merge_data_pin(u8 value, u8 mask, u64 cycle) const
{
	if (!m_model.sync_data_selected())
		return value;
	bool const high = m_model.sync_data_high();
	if (m_pin_trace && (m_model.status_lines() & 1U))
		osd_printf_info("smart-card read %llu bit=%zu value=%u\n",
			static_cast<unsigned long long>(cycle), m_model.sync_bit_address(), unsigned(high));
	return u8((value & ~mask) | (high ? mask : 0U));
}
