// SPDX-License-Identifier: GPL-2.0-or-later

#include "millennium_z180_internal.h"

#include "millennium_z180_register_math.h"

namespace {

// From third-party/mame Z180 (z180.cpp): DSTAT read merges in ones for non-readable bits.
constexpr std::uint8_t Z180_DSTAT_READ_MERGE = 0xfd;
constexpr std::uint8_t Z180_DMODE_READ_MERGE = 0x3e;

} // namespace

bool millennium_z180_port_is_internal_window(z180_device &cpu, std::uint16_t port_full)
{
	std::uint8_t const iocr = std::uint8_t(cpu.state_int(Z180_IOCR) & 0xff);
	// Match z180_device::is_internal_io_address with extended_io == false (Z80180).
	return (((port_full ^ iocr) & 0xffc0) == 0);
}

char const *millennium_z180_internal_trace_tag(std::uint8_t const port_low6)
{
	switch (port_low6) {
	case 0x00:
		return "z180_asci_cntla0";
	case 0x01:
		return "z180_asci_cntla1";
	case 0x02:
		return "z180_asci_cntlb0";
	case 0x03:
		return "z180_asci_cntlb1";
	case 0x04:
		return "z180_asci_stat0";
	case 0x05:
		return "z180_asci_stat1";
	case 0x06:
		return "z180_asci_tdr0";
	case 0x07:
		return "z180_asci_tdr1";
	case 0x08:
		return "z180_asci_rdr0";
	case 0x09:
		return "z180_asci_rdr1";
	case 0x0a:
		return "z180_csio_cntr";
	case 0x0b:
		return "z180_csio_trdr";
	case 0x0c:
		return "z180_prt_tmdr0l";
	case 0x0d:
		return "z180_prt_tmdr0h";
	case 0x0e:
		return "z180_prt_rldr0l";
	case 0x0f:
		return "z180_prt_rldr0h";
	case 0x10:
		return "z180_prt_tcr";
	case 0x14:
		return "z180_prt_tmdr1l";
	case 0x15:
		return "z180_prt_tmdr1h";
	case 0x16:
		return "z180_prt_rldr1l";
	case 0x17:
		return "z180_prt_rldr1h";
	case 0x18:
		return "z180_frc";
	case 0x20:
		return "z180_dma_sar0l";
	case 0x21:
		return "z180_dma_sar0h";
	case 0x22:
		return "z180_dma_sar0b";
	case 0x23:
		return "z180_dma_dar0l";
	case 0x24:
		return "z180_dma_dar0h";
	case 0x25:
		return "z180_dma_dar0b";
	case 0x26:
		return "z180_dma_bcr0l";
	case 0x27:
		return "z180_dma_bcr0h";
	case 0x28:
		return "z180_dma_mar1l";
	case 0x29:
		return "z180_dma_mar1h";
	case 0x2a:
		return "z180_dma_mar1b";
	case 0x2b:
		return "z180_dma_iar1l";
	case 0x2c:
		return "z180_dma_iar1h";
	case 0x2d:
		return "z180_dma_iar1b";
	case 0x2e:
		return "z180_dma_bcr1l";
	case 0x2f:
		return "z180_dma_bcr1h";
	case 0x30:
		return "z180_dma_dstat";
	case 0x31:
		return "z180_dma_dmode";
	case 0x32:
		return "z180_dma_dcntl";
	case 0x33:
		return "z180_il";
	case 0x34:
		return "z180_itc";
	case 0x36:
		return "z180_rcr";
	case 0x38:
		return "z180_mmu_cbr";
	case 0x39:
		return "z180_mmu_bbr";
	case 0x3a:
		return "z180_mmu_cbar";
	case 0x3e:
		return "z180_omcr";
	case 0x3f:
		return "z180_iocr";
	default:
		return "z180_internal_unknown";
	}
}

std::uint8_t millennium_z180_trace_read_byte(z180_device &cpu, std::uint16_t port_full)
{
	std::uint8_t const p = port_full & 0x3f;

	if (!millennium_z180_port_is_internal_window(cpu, port_full))
		return 0xff;

	switch (p) {
	case 0x00:
		return std::uint8_t(cpu.state_int(Z180_CNTLA0) & 0xff);
	case 0x01:
		return std::uint8_t(cpu.state_int(Z180_CNTLA1) & 0xff);
	case 0x02:
		return std::uint8_t(cpu.state_int(Z180_CNTLB0) & 0xff);
	case 0x03:
		return std::uint8_t(cpu.state_int(Z180_CNTLB1) & 0xff);
	case 0x04:
		return std::uint8_t(cpu.state_int(Z180_STAT0) & 0xff);
	case 0x05:
		return std::uint8_t(cpu.state_int(Z180_STAT1) & 0xff);
	case 0x06:
		return std::uint8_t(cpu.state_int(Z180_TDR0) & 0xff);
	case 0x07:
		return std::uint8_t(cpu.state_int(Z180_TDR1) & 0xff);
	case 0x08:
		return std::uint8_t(cpu.state_int(Z180_RDR0) & 0xff);
	case 0x09:
		return std::uint8_t(cpu.state_int(Z180_RDR1) & 0xff);
	case 0x0a:
		return std::uint8_t(cpu.state_int(Z180_CNTR) & 0xff);
	case 0x0b:
		return std::uint8_t(cpu.state_int(Z180_TRDR) & 0xff);
	case 0x0c:
		return std::uint8_t(cpu.state_int(Z180_TMDR0) & 0xff);
	case 0x0d:
		return std::uint8_t((cpu.state_int(Z180_TMDR0) >> 8) & 0xff);
	case 0x0e:
		return std::uint8_t(cpu.state_int(Z180_RLDR0) & 0xff);
	case 0x0f:
		return std::uint8_t((cpu.state_int(Z180_RLDR0) >> 8) & 0xff);
	case 0x10:
		return std::uint8_t(cpu.state_int(Z180_TCR) & 0xff);
	case 0x14:
		return std::uint8_t(cpu.state_int(Z180_TMDR1) & 0xff);
	case 0x15:
		return std::uint8_t((cpu.state_int(Z180_TMDR1) >> 8) & 0xff);
	case 0x16:
		return std::uint8_t(cpu.state_int(Z180_RLDR1) & 0xff);
	case 0x17:
		return std::uint8_t((cpu.state_int(Z180_RLDR1) >> 8) & 0xff);
	case 0x18:
		return std::uint8_t(cpu.state_int(Z180_FRC) & 0xff);
	case 0x34:
		return millennium_z180_itc_read_byte(std::uint8_t(cpu.state_int(Z180_ITC) & 0xff));
	case 0x36:
		return millennium_z180_rcr_read_byte(std::uint8_t(cpu.state_int(Z180_RCR) & 0xff));
	case 0x38:
		return std::uint8_t(cpu.state_int(Z180_CBR) & 0xff);
	case 0x39:
		return std::uint8_t(cpu.state_int(Z180_BBR) & 0xff);
	case 0x3a:
		return std::uint8_t(cpu.state_int(Z180_CBAR) & 0xff);
	case 0x3f:
		return millennium_z180_iocr_read_byte(std::uint8_t(cpu.state_int(Z180_IOCR) & 0xff), false);
	case 0x30:
		return std::uint8_t((cpu.state_int(Z180_DSTAT) & 0xff) | ~Z180_DSTAT_READ_MERGE);
	case 0x31:
		return std::uint8_t((cpu.state_int(Z180_DMODE) & 0xff) | ~Z180_DMODE_READ_MERGE);
	case 0x20:
		return std::uint8_t(cpu.state_int(Z180_SAR0) & 0xff);
	case 0x21:
		return std::uint8_t((cpu.state_int(Z180_SAR0) >> 8) & 0xff);
	case 0x22:
		return std::uint8_t((cpu.state_int(Z180_SAR0) >> 16) & 0x0f);
	case 0x23:
		return std::uint8_t(cpu.state_int(Z180_DAR0) & 0xff);
	case 0x24:
		return std::uint8_t((cpu.state_int(Z180_DAR0) >> 8) & 0xff);
	case 0x25:
		return std::uint8_t((cpu.state_int(Z180_DAR0) >> 16) & 0x0f);
	case 0x26:
		return std::uint8_t(cpu.state_int(Z180_BCR0) & 0xff);
	case 0x27:
		return std::uint8_t((cpu.state_int(Z180_BCR0) >> 8) & 0xff);
	case 0x28:
		return std::uint8_t(cpu.state_int(Z180_MAR1) & 0xff);
	case 0x29:
		return std::uint8_t((cpu.state_int(Z180_MAR1) >> 8) & 0xff);
	case 0x2a:
		return std::uint8_t((cpu.state_int(Z180_MAR1) >> 16) & 0x0f);
	case 0x2b:
		return std::uint8_t(cpu.state_int(Z180_IAR1) & 0xff);
	case 0x2c:
		return std::uint8_t((cpu.state_int(Z180_IAR1) >> 8) & 0xff);
	case 0x2d:
		return std::uint8_t((cpu.state_int(Z180_IAR1) >> 16) & 0x0f);
	case 0x2e:
		return std::uint8_t(cpu.state_int(Z180_BCR1) & 0xff);
	case 0x2f:
		return std::uint8_t((cpu.state_int(Z180_BCR1) >> 8) & 0xff);
	case 0x3e:
		return millennium_z180_omcr_read_byte(std::uint8_t(cpu.state_int(Z180_OMCR) & 0xff));
	case 0x32:
		return std::uint8_t(cpu.state_int(Z180_DCNTL) & 0xff);
	case 0x33:
		return millennium_z180_il_read_byte(std::uint8_t(cpu.state_int(Z180_IL) & 0xff));
	default:
		break;
	}
	return 0xff;
}
