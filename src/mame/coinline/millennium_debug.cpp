// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_debug.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

std::string millennium_boot_trace_timestamp_utc()
{
	std::time_t const t = std::time(nullptr);
	std::tm tm{};
#if defined(_WIN32)
	if (gmtime_s(&tm, &t) != 0)
		return "1970-01-01T00:00:00Z";
#else
	if (!gmtime_r(&t, &tm))
		return "1970-01-01T00:00:00Z";
#endif
	char buf[64];
	std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
	return buf;
}

void millennium_boot_trace_append_line(std::filesystem::path const &path, std::string const &json_object)
{
	// std::ofstream(path) on Windows uses wide underlying APIs; narrow fopen() can fail for some paths.
	std::ofstream f(path, std::ios::binary | std::ios::out | std::ios::app);
	if (!f)
		return;
	if (!json_object.empty())
		f.write(json_object.data(), std::streamsize(json_object.size()));
	f.put('\n');
}

std::string millennium_format_io_trace_line(std::uint64_t cycle, std::uint16_t port, char rw, std::uint8_t data,
	char const *tag)
{
	char buf[384];
	char const *safe_tag = tag ? tag : "";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"port\":\"0x%04X\",\"rw\":\"%c\",\"data\":\"0x%02X\",\"tag\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(port), rw, unsigned(data), safe_tag);
	return std::string(buf);
}

std::string millennium_format_io_trace_line_v2(std::uint64_t cycle, std::uint16_t port, char rw, std::uint8_t data,
	char const *tag, std::uint16_t pc, std::uint16_t sp, char const *milestone)
{
	char buf[512];
	char const *safe_tag = tag ? tag : "";
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"port\":\"0x%04X\",\"rw\":\"%c\",\"data\":\"0x%02X\",\"tag\":\"%s\","
		"\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(port), rw, unsigned(data), safe_tag, unsigned(pc), unsigned(sp),
		ms);
	return std::string(buf);
}

std::string millennium_format_memory_trace_line(std::uint64_t cycle, std::uint32_t phys_addr, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, char const *milestone)
{
	char buf[256];
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"addr\":\"0x%05X\",\"data\":\"0x%02X\",\"pc\":\"0x%04X\",\"sp\":\"0x%04X\","
		"\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(phys_addr), unsigned(data), unsigned(pc), unsigned(sp), ms);
	return std::string(buf);
}

std::string millennium_format_nvram_storage_trace_line(std::uint64_t cycle, std::uint32_t region_offset,
	std::uint32_t phys_addr, char rw, std::uint8_t data, std::uint16_t pc, std::uint16_t sp, char const *milestone)
{
	char buf[320];
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"event\":\"nvram_storage\",\"region_offset\":%u,\"phys_addr\":\"0x%05X\",\"rw\":\"%c\","
		"\"data\":\"0x%02X\",\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(region_offset), unsigned(phys_addr), rw, unsigned(data),
		unsigned(pc), unsigned(sp), ms);
	return std::string(buf);
}

std::string millennium_format_microwire_trace_line(std::uint64_t cycle, char const *op, std::uint16_t word_addr,
	std::uint16_t word_be, std::uint16_t pc, std::uint16_t sp, char const *milestone)
{
	char buf[384];
	char const *ms = milestone && *milestone ? milestone : "none";
	char const *safe_op = op && *op ? op : "unknown";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"event\":\"microwire_eeprom\",\"op\":\"%s\",\"word_addr\":\"0x%02X\",\"word_be\":\"0x%04X\","
		"\"pc\":\"0x%04X\",\"sp\":\"0x%04X\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), safe_op, unsigned(word_addr), unsigned(word_be), unsigned(pc), unsigned(sp),
		ms);
	return std::string(buf);
}

std::string millennium_format_cpu_trace_line(std::uint64_t cycle, std::uint16_t pc, std::uint8_t op0, std::uint8_t op1,
	std::uint8_t op2, std::uint16_t sp, bool iff1, char const *milestone)
{
	char buf[320];
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"op\":[\"0x%02X\",\"0x%02X\",\"0x%02X\"],\"sp\":\"0x%04X\","
		"\"iff1\":%s,\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(pc), unsigned(op0), unsigned(op1), unsigned(op2), unsigned(sp),
		iff1 ? "true" : "false", ms);
	return std::string(buf);
}

std::string millennium_format_z180_reg_trace_line(std::uint64_t cycle, std::uint16_t pc, std::uint16_t sp, bool iff1,
	bool iff2, millennium_z180_snapshot const &s, char const *milestone)
{
	char const *ms = milestone && *milestone ? milestone : "none";
	char pcbuf[16], spbuf[16];
	std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
	std::snprintf(spbuf, sizeof(spbuf), "0x%04X", unsigned(sp));
	std::ostringstream os;
	os << "{\"cycle\":" << cycle << ",\"pc\":\"" << pcbuf << "\",\"sp\":\"" << spbuf << "\",";
	os << "\"iff1\":" << (iff1 ? "true" : "false") << ",\"iff2\":" << (iff2 ? "true" : "false") << ',';
	os << "\"milestone\":\"" << ms << "\",";
	os << "\"cbr\":\"0x" << std::hex << std::uppercase << unsigned(s.cbr) << std::dec << "\",";
	os << "\"bbr\":\"0x" << std::hex << std::uppercase << unsigned(s.bbr) << std::dec << "\",";
	os << "\"cbar\":\"0x" << std::hex << std::uppercase << unsigned(s.cbar) << std::dec << "\",";
	os << "\"rcr\":\"0x" << std::hex << std::uppercase << unsigned(s.rcr) << std::dec << "\",";
	os << "\"cntla0\":\"0x" << std::hex << std::uppercase << unsigned(s.cntla0) << std::dec << "\",";
	os << "\"cntlb0\":\"0x" << std::hex << std::uppercase << unsigned(s.cntlb0) << std::dec << "\",";
	os << "\"stat0\":\"0x" << std::hex << std::uppercase << unsigned(s.stat0) << std::dec << "\",";
	os << "\"tcr\":\"0x" << std::hex << std::uppercase << unsigned(s.tcr) << std::dec << "\",";
	os << "\"rldr0\":\"0x" << std::hex << std::uppercase << unsigned(s.rldr0) << std::dec << "\",";
	os << "\"tmdr0\":\"0x" << std::hex << std::uppercase << unsigned(s.tmdr0) << std::dec << "\",";
	os << "\"il\":\"0x" << std::hex << std::uppercase << unsigned(s.il) << std::dec << "\",";
	os << "\"itc\":\"0x" << std::hex << std::uppercase << unsigned(s.itc) << std::dec << "\",";
	os << "\"dstat\":\"0x" << std::hex << std::uppercase << unsigned(s.dstat) << std::dec << "\",";
	os << "\"dmode\":\"0x" << std::hex << std::uppercase << unsigned(s.dmode) << std::dec << "\",";
	os << "\"dcntl\":\"0x" << std::hex << std::uppercase << unsigned(s.dcntl) << std::dec << "\",";
	os << "\"iocr\":\"0x" << std::hex << std::uppercase << unsigned(s.iocr) << std::dec << "\"}";
	return os.str();
}

std::string millennium_format_stack_trace_line(std::uint64_t cycle, std::uint32_t phys_addr, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, std::uint32_t sp_phys20, char const *region_tag, char const *milestone)
{
	char const *ms = milestone && *milestone ? milestone : "none";
	char const *rt = region_tag && *region_tag ? region_tag : "unknown";
	char buf[420];
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"phys\":\"0x%05X\",\"data\":\"0x%02X\",\"pc\":\"0x%04X\",\"sp\":\"0x%04X\","
		"\"sp_phys\":\"0x%05X\",\"region\":\"%s\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(phys_addr), unsigned(data), unsigned(pc), unsigned(sp),
		unsigned(sp_phys20 & 0xfffffU), rt, ms);
	return std::string(buf);
}

std::string millennium_format_ram_init_trace_line(std::uint64_t cycle, std::uint32_t phys_addr, std::uint8_t data,
	std::uint16_t pc, std::uint16_t sp, char const *region_tag, char const *milestone)
{
	char const *ms = milestone && *milestone ? milestone : "none";
	char const *rt = region_tag && *region_tag ? region_tag : "unknown";
	char buf[360];
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"phys\":\"0x%05X\",\"data\":\"0x%02X\",\"pc\":\"0x%04X\",\"sp\":\"0x%04X\","
		"\"region\":\"%s\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(phys_addr), unsigned(data), unsigned(pc), unsigned(sp), rt, ms);
	return std::string(buf);
}

std::string millennium_format_mmu_translation_trace_line(std::uint64_t cycle, std::uint16_t pc_logical,
	std::uint16_t sp_logical, std::uint8_t cbr, std::uint8_t bbr, std::uint8_t cbar, std::uint32_t pc_phys20,
	std::uint32_t sp_phys20, char const *milestone)
{
	char const *ms = milestone && *milestone ? milestone : "none";
	char buf[400];
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"pc_log\":\"0x%04X\",\"sp_log\":\"0x%04X\","
		"\"cbr\":\"0x%02X\",\"bbr\":\"0x%02X\",\"cbar\":\"0x%02X\","
		"\"pc_phys\":\"0x%05X\",\"sp_phys\":\"0x%05X\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(pc_logical), unsigned(sp_logical), unsigned(cbr), unsigned(bbr),
		unsigned(cbar), unsigned(pc_phys20 & 0xfffffU), unsigned(sp_phys20 & 0xfffffU), ms);
	return std::string(buf);
}

std::string millennium_boot_trace_m0(std::string const &ts, std::string const &sha256_hex,
	std::uint64_t size_bytes)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M0\",\"ts\":\"" << ts << "\",\"sha256\":\"" << sha256_hex << "\",\"size\":"
	   << size_bytes << '}';
	return os.str();
}

std::string millennium_boot_trace_m1(std::string const &ts, std::uint16_t pc, std::string const &opcode_hex)
{
	char pcbuf[16];
	std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
	std::ostringstream os;
	os << "{\"milestone\":\"M1\",\"ts\":\"" << ts << "\",\"pc\":\"" << pcbuf << "\",\"opcode\":\"" << opcode_hex
	   << "\"}";
	return os.str();
}

std::string millennium_boot_trace_m2(std::string const &ts, std::uint16_t pc)
{
	char pcbuf[16];
	std::snprintf(pcbuf, sizeof(pcbuf), "0x%04X", unsigned(pc));
	std::ostringstream os;
	os << "{\"milestone\":\"M2\",\"ts\":\"" << ts << "\",\"pc\":\"" << pcbuf << "\"}";
	return os.str();
}

std::string millennium_boot_trace_m3(std::string const &ts, std::uint64_t ram_writes, std::uint16_t sp,
	char const *m3_trigger)
{
	char spbuf[16];
	std::snprintf(spbuf, sizeof(spbuf), "0x%04X", unsigned(sp));
	char const *tr = m3_trigger && *m3_trigger ? m3_trigger : "unspecified";
	std::ostringstream os;
	os << "{\"milestone\":\"M3\",\"ts\":\"" << ts << "\",\"ram_writes\":" << ram_writes << ",\"sp\":\""
	   << spbuf << "\",\"m3_trigger\":\"" << tr << "\"}";
	return os.str();
}

namespace {

void append_json_quoted_string(std::ostringstream &os, std::string const &s)
{
	os << '"';
	for (char c : s) {
		if (c == '"' || c == '\\')
			os << '\\' << c;
		else if (static_cast<unsigned char>(c) < 0x20)
			os << ' ';
		else
			os << c;
	}
	os << '"';
}

} // namespace

std::string millennium_boot_trace_m5(std::string const &ts, std::string const &device, std::string const &pc_hex)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M5\",\"ts\":\"" << ts << "\",\"device\":";
	append_json_quoted_string(os, device);
	os << ",\"pc\":";
	append_json_quoted_string(os, pc_hex);
	os << '}';
	return os.str();
}

std::string millennium_boot_trace_m5v(std::string const &ts, std::string const &pc_hex, std::string const &phrase_hex)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M5V\",\"ts\":\"" << ts << "\",\"device\":\"voiceware\",\"port\":\"0x0061\",\"phrase\":";
	append_json_quoted_string(os, phrase_hex);
	os << ",\"pc\":";
	append_json_quoted_string(os, pc_hex);
	os << '}';
	return os.str();
}

std::string millennium_boot_trace_m5a(std::string const &ts, std::string const &pc_hex, std::string const &phrase_hex)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M5A\",\"ts\":\"" << ts << "\",\"device\":\"upd7759\",\"port\":\"0x0061\",\"phrase\":";
	append_json_quoted_string(os, phrase_hex);
	os << ",\"pc\":";
	append_json_quoted_string(os, pc_hex);
	os << '}';
	return os.str();
}

std::string millennium_boot_trace_m5c(std::string const &ts, std::string const &pc_hex)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M5C\",\"ts\":\"" << ts << "\",\"device\":\"upd7759\",\"pc\":";
	append_json_quoted_string(os, pc_hex);
	os << ",\"note\":\"chip_idle\"}";
	return os.str();
}

std::string millennium_boot_trace_m6(std::string const &ts, std::string const &vfd_summary)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M6\",\"ts\":\"" << ts << "\",\"vfd\":";
	append_json_quoted_string(os, vfd_summary);
	os << '}';
	return os.str();
}

std::string millennium_boot_trace_m7(std::string const &ts, std::uint64_t keypad_scan_count)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M7\",\"ts\":\"" << ts << "\",\"keypad_scan_count\":" << keypad_scan_count << '}';
	return os.str();
}

std::string millennium_boot_trace_m8(std::string const &ts, std::string const &asci_summary, bool dcd, bool cts)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M8\",\"ts\":\"" << ts << "\",\"asci_state\":";
	append_json_quoted_string(os, asci_summary);
	os << ",\"dcd\":" << (dcd ? "true" : "false") << ",\"cts\":" << (cts ? "true" : "false") << '}';
	return os.str();
}

std::string millennium_boot_trace_m9(std::string const &ts, std::string const &scheduler_pc_hex)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M9\",\"ts\":\"" << ts << "\",\"scheduler_pc\":";
	append_json_quoted_string(os, scheduler_pc_hex);
	os << '}';
	return os.str();
}

std::string millennium_boot_trace_m10(std::string const &ts, std::string const &vfd_summary)
{
	std::ostringstream os;
	os << "{\"milestone\":\"M10\",\"ts\":\"" << ts << "\",\"vfd\":";
	append_json_quoted_string(os, vfd_summary);
	os << ",\"idle\":true}";
	return os.str();
}

std::string millennium_format_vector_event_line(std::uint64_t cycle, char const *event, std::uint16_t pc,
	std::uint16_t sp, std::uint8_t op0, std::uint8_t op1, bool iff1, int im, char const *milestone)
{
	char buf[420];
	char const *ev = event && *event ? event : "unknown";
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"event\":\"%s\",\"pc\":\"0x%04X\",\"sp\":\"0x%04X\","
		"\"op\":[\"0x%02X\",\"0x%02X\"],\"iff1\":%s,\"im\":%d,\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), ev, unsigned(pc), unsigned(sp), unsigned(op0), unsigned(op1),
		iff1 ? "true" : "false", im, ms);
	return std::string(buf);
}

std::string millennium_format_eidi_event_line(std::uint64_t cycle, std::uint16_t pc, std::uint8_t opcode0,
	std::uint8_t opcode1, char const *mnemonic, std::uint16_t sp, bool iff1_before, bool iff2_before,
	bool iff1_after, bool iff2_after, char const *nearby_symbol, char const *note, char const *milestone)
{
	char buf[768];
	char const *m = mnemonic && *mnemonic ? mnemonic : "unknown";
	char const *sym = nearby_symbol && *nearby_symbol ? nearby_symbol : "unknown";
	char const *n = note && *note ? note : "";
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"pc\":\"0x%04X\",\"opcode\":\"0x%02X\",\"opcode_1\":\"0x%02X\",\"mnemonic\":\"%s\","
		"\"sp\":\"0x%04X\",\"iff1_before\":%s,\"iff2_before\":%s,\"iff1_after\":%s,\"iff2_after\":%s,"
		"\"nearby_symbol\":\"%s\",\"note\":\"%s\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), unsigned(pc), unsigned(opcode0), unsigned(opcode1), m, unsigned(sp),
		iff1_before ? "true" : "false", iff2_before ? "true" : "false", iff1_after ? "true" : "false",
		iff2_after ? "true" : "false", sym, n, ms);
	return std::string(buf);
}

std::string millennium_format_voiceware_trace_line(std::uint64_t cycle, char rw, std::uint8_t data, std::uint16_t pc,
	std::uint16_t sp, std::uint8_t hw_cntl_shadow, char const *milestone)
{
	char buf[360];
	char const *ms = milestone && *milestone ? milestone : "none";
	std::snprintf(buf, sizeof(buf),
		"{\"cycle\":%llu,\"port\":\"0x0061\",\"rw\":\"%c\",\"data\":\"0x%02X\",\"pc\":\"0x%04X\",\"sp\":\"0x%04X\","
		"\"hw_cntl_shadow\":\"0x%02X\",\"milestone\":\"%s\"}",
		static_cast<unsigned long long>(cycle), rw, unsigned(data), unsigned(pc), unsigned(sp),
		unsigned(hw_cntl_shadow), ms);
	return std::string(buf);
}
