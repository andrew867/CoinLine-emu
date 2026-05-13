// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_snapshot.h"

#include <cstdio>
#include <sstream>

std::string millennium_hex_byte(std::uint8_t v)
{
	char buf[8];
	std::snprintf(buf, sizeof(buf), "0x%02X", unsigned(v));
	return buf;
}

std::string millennium_hex_word(std::uint16_t v)
{
	char buf[8];
	std::snprintf(buf, sizeof(buf), "0x%04X", unsigned(v));
	return buf;
}

std::string millennium_format_boot_m4(std::string const &ts, millennium_z180_snapshot const &s)
{
	std::ostringstream mmu;
	mmu << "CBR=" << millennium_hex_byte(s.cbr) << " BBR=" << millennium_hex_byte(s.bbr) << " CBAR="
	    << millennium_hex_byte(s.cbar) << " RCR=" << millennium_hex_byte(s.rcr) << " IOCR="
	    << millennium_hex_byte(s.iocr);

	std::ostringstream asci;
	asci << "CNTLA0=" << millennium_hex_byte(s.cntla0) << " CNTLB0=" << millennium_hex_byte(s.cntlb0) << " STAT0="
	     << millennium_hex_byte(s.stat0);

	std::ostringstream prt;
	prt << "TCR=" << millennium_hex_byte(s.tcr) << " RLDR0=" << millennium_hex_word(s.rldr0) << " TMDR0="
	    << millennium_hex_word(s.tmdr0);

	std::ostringstream intr;
	intr << "IL=" << millennium_hex_byte(s.il) << " ITC=" << millennium_hex_byte(s.itc);

	std::ostringstream dma;
	dma << "DSTAT=" << millennium_hex_byte(s.dstat) << " DMODE=" << millennium_hex_byte(s.dmode) << " DCNTL="
	    << millennium_hex_byte(s.dcntl);

	std::ostringstream os;
	os << "{\"milestone\":\"M4\",\"ts\":\"" << ts << "\",\"registers\":{";
	os << "\"mmu\":\"" << mmu.str() << "\",";
	os << "\"asci\":\"" << asci.str() << "\",";
	os << "\"prt\":\"" << prt.str() << "\",";
	os << "\"int\":\"" << intr.str() << "\",";
	os << "\"dma\":\"" << dma.str() << '"';
	os << "}}";
	return os.str();
}
