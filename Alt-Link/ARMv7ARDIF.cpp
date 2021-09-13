#include "stdafx.h"
#include "ARMv7ARDIF.h"

#define _DBGPRT printf

#define REG_DBGDIDR		(base + 0x000)
#define REG_DBGDTRRX	(base + 0x080)	/* 32 */
#define REG_DBGDTRTX	(base + 0x08C)	/* 35 */
#define REG_DBGDSCR		(base + 0x088)	/* 34 */
#define REG_DBGITR		(base + 0x084)	/* 33 */
#define REG_DBGPCSR_33	(base + 0x084)	/* 33 */
#define REG_DBGDRCR		(base + 0x090)
#define REG_DBGPCSR_40	(base + 0x0A0)	/* 40 */
#define REG_DBGCIDSR	(base + 0x0A4)	/* 41 */
#define REG_DBGPRSR		(base + 0x314)
#define REG_MIDR		(base + 0xD00)
#define REG_MPIDR		(base + 0xD14)
#define REG_DBGDEVID1	(base + 0xFC4)
#define REG_DBGDEVID	(base + 0xFC8)

union DBGDSCR
{
	struct
	{
		uint32_t HALTED			: 1;
		uint32_t RESTARTED		: 1;
		uint32_t MOE			: 4;
		uint32_t SDABORT_l		: 1;
		uint32_t ADABORT_l		: 1;
		uint32_t UND_l			: 1;
		uint32_t RESERVED		: 1;
		uint32_t DBGack			: 1;
		uint32_t INTdis			: 1;
		uint32_t UDCCdis		: 1;
		uint32_t ITRen			: 1;
		uint32_t HDBGen			: 1;
		uint32_t MDBGen			: 1;
		uint32_t SPIDdis		: 1;
		uint32_t SPNIDdis		: 1;
		uint32_t NS				: 1;
		uint32_t ADAdiscard		: 1;
		uint32_t ExtDCCmode		: 2;
		uint32_t ZERO_0			: 2;
		uint32_t InstrCompl_l	: 1;
		uint32_t PipeAdv		: 1;
		uint32_t TXfull_l		: 1;
		uint32_t RXfull_l		: 1;
		uint32_t ZERO_1			: 1;
		uint32_t TXfull			: 1;
		uint32_t RXfull			: 1;
		uint32_t ZERO_2			: 1;
	};
	uint32_t raw;
	void print();
	void printIfNotSame();
};
static_assert(CONFIRM_UINT32(DBGDSCR));

union DBGDRCR
{
	struct
	{
		uint32_t HRQ	: 1;
		uint32_t RRQ	: 1;
		uint32_t CSE	: 1;
		uint32_t CSPA	: 1;
		uint32_t CBRRQ	: 1;
		uint32_t SBZ	: 27;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DBGDRCR));

union DBGPCSR
{
	struct
	{
		uint32_t T		: 1;
		uint32_t PCS	: 31;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DBGPCSR));

void DBGDSCR::print()
{
	_DBGPRT("  DSCR           : 0x%08x\n", raw);
	_DBGPRT("    HALTED       : 0x%08x %s\n", HALTED,
		HALTED == 0 ? "" :
		MOE == 0 ? "(Halt request debug event)" :
		MOE == 1 ? "(Breakpoint debug event)" :
		MOE == 2 ? "(Asynchronous watchpoint debug event)" :
		MOE == 3 ? "(BKPT instruction debug event)" :
		MOE == 4 ? "(External debug request debug event)" :
		MOE == 5 ? "(Vector catch debug event)" :
		MOE == 8 ? "(OS Unlock catch debug event)" :
		MOE == 10 ? "(Synchronous watchpoint debug event)" :
		"(Reserved)");
	_DBGPRT("    RESTARTED    : 0x%08x\n", RESTARTED);
	_DBGPRT("    SDABORT_l(%x) ADABORT_l(%x) UND_l(%x) DBGack(%x) INTdis(%x)\n", SDABORT_l, ADABORT_l, UND_l, DBGack, INTdis);
	_DBGPRT("    UDCCdis(%x) ITRen(%x) HDBGen(%x) MDBGen(%x)\n", UDCCdis, ITRen, HDBGen, MDBGen);
	_DBGPRT("    SPIDdis(%x) SPNIDdis(%x) NS(%x) ADAdiscard(%x)\n", SPIDdis, SPNIDdis, NS, ADAdiscard);
	_DBGPRT("    InstrCompl_l(%x) PipeAdv(%x) TXfull_l(%x) RXfull_l(%x)\n", InstrCompl_l, PipeAdv, TXfull_l, RXfull_l);
	_DBGPRT("    TXfull(%x) RXfull(%x)\n", TXfull, RXfull);
	_DBGPRT("    ExtDCCmode   : %s\n",
		ExtDCCmode == 0 ? "Non-blocking mode" :
		ExtDCCmode == 1 ? "Stall mode" :
		ExtDCCmode == 2 ? "Fast mode" :
		"Reserved");
}

void DBGDSCR::printIfNotSame()
{
	static DBGDSCR lastDscr = { };
	if (lastDscr.raw != raw)
	{
		print();
		lastDscr.raw = raw;
	}
}

void ARMv7ARDIF::MPIDR::print()
{
	_DBGPRT("  MPIDR          : 0x%08x\n", raw);

	if (RAO == 0)
	{
		_DBGPRT("    No multiprocessor extension\n");
		return;
	}

	if (U)
		_DBGPRT("    Uniprocessor System\n");
	else
		_DBGPRT("    Multiprocessor System\n");

	_DBGPRT("    CLUSTER ID   : %d, CPU ID : %d\n", CLUSTERID, CPUID);
}

void ARMv7ARDIF::DBGDIDR::print()
{
	_DBGPRT("  DBGDIDR        : 0x%08x\n", raw);
	_DBGPRT("    Revision     : 0x%08x\n", Revision);
	_DBGPRT("    Variant      : 0x%08x\n", Variant);
	_DBGPRT("    Implement    : SE(%x) PCSR(%x) nSUHD(%x) DEVID(%x)\n", SE_imp, PCSR_imp, nSUHD_imp, DEVID_imp);
	_DBGPRT("    Version      : %s\n",
		Version == ARMv6_0 ? "ARMv6 Debug architecture" :
		Version == ARMv6_1 ? "ARMv6.1 Debug architecture" :
		Version == ARMv7_ALL ? "ARMv7 Debug architecture with all CP14 registers" :
		Version == ARMv7_BASIC ? "ARMv6 Debug architecture with baseline CP14 registers" :
		Version == ARMv7_1 ? "ARMv7.1 Debug architecture" : "UNKNOWN");
	if (BRPs == 0)
		_DBGPRT("    Breakpoints  : INVALID(%d)\n", BRPs);
	else
		_DBGPRT("    Breakpoints  : %d\n", BRPs + 1);
	_DBGPRT("      Context matching : %d\n", CTX_CMPs);
	_DBGPRT("    Watchpoints  : %d\n", WRPs + 1);
}

bool ARMv7ARDIF::DBGDIDR::DEVID1_Exists()
{
	if (Version == ARMv7_1)
		return true;

	if (DEVID_imp)
		if (Version == ARMv7_ALL || Version == ARMv7_BASIC)
			return true;

	return false;
}

errno_t ARMv7ARDIF::init(uint32_t _PART)
{
	PART = _PART;

	errno_t ret = ap.read(REG_MPIDR, &mpidr.raw);
	if (ret != OK)
		return ret;
	mpidr.print();

	ret = ap.read(REG_DBGDIDR, &didr.raw);
	if (ret != OK)
		return ret;
	didr.print();

	if (didr.DEVID_Exists())
	{
		ret = ap.read(REG_DBGDEVID, &devid.raw);
		if (ret != OK)
			return ret;

		if (didr.DEVID1_Exists())
		{
			ret = ap.read(REG_DBGDEVID1, &devid1.raw);
			if (ret != OK)
				return ret;
		}
	}
	return OK;
}

errno_t ARMv7ARDIF::readReg(uint32_t reg, uint32_t* data)
{
	if (data == nullptr)
		return EINVAL;

	if (reg > 14)
		return EINVAL;

	// MCR p14, 0, Rd, c0, c5, 0
	errno_t ret = writeITR(0xEE000E15 + (reg << 12));
	if (ret != OK)
		return ret;

	ret = readDCC(data);
	if (ret != OK)
		return ret;

	return OK;
}

errno_t ARMv7ARDIF::writeReg(uint32_t reg, uint32_t data)
{
	if (reg > 14)
		return EINVAL;

	errno_t ret = writeDCC(data);
	if (ret != OK)
		return ret;

	// MRC p14, 0, Rd, c0, c5, 0
	ret = writeITR(0xEE100E15 + (reg << 12));
	if (ret != OK)
		return ret;

	return OK;
}

errno_t ARMv7ARDIF::getPC(uint32_t *pc)
{
	if (pc == nullptr)
		return EINVAL;

	uint32_t r0 = 0;
	errno_t ret = readReg(0, &r0);
	if (ret != OK)
		return ret;

	// MOV r0, pc
	ret = writeITR(0xE1A0000F);
	if (ret != OK)
		return ret;

	ret = readReg(0, pc);
	if (ret != OK)
		return ret;

	ret = writeReg(0, r0);
	if (ret != OK)
		return ret;

	return OK;
}

errno_t ARMv7ARDIF::getPCSR(uint32_t *pc)
{
	if (pc == nullptr)
		return EINVAL;

	errno_t ret;
	bool found = false;
	bool offset = true;
	DBGPCSR pcsr;

	if (PART == ADIv5::Component::ARM_PART_DEBUG_IF_A9)
	{
		offset = false;
	}
	else
	{
		if (didr.DEVID1_Exists())
		{
			if (devid1.PCSROffset == DBGDEVID1::NO_OFFSET)
				offset = true;
			else
				offset = false;
		}
	}

	// try to read from reg40
	if (didr.DEVID_Exists())
	{
		if (devid.PCSR_Exists())
		{
			ret = ap.read(REG_DBGPCSR_40, &pcsr.raw);
			if (ret != OK)
				return ret;
			found = true;
		}
	}

	// try to read from reg33 if reg40 is not found
	if (found == false)
	{
		if (didr.PCSR_imp)
		{
			ret = ap.read(REG_DBGPCSR_33, &pcsr.raw);
			if (ret != OK)
				return ret;
			found = true;
		}
	}

	if (found == false)
		return ENODATA;

	if (pcsr.raw == 0xFFFFFFFF)
	{
		*pc = pcsr.raw;
		return OK;
	}

	// warn not supported format
	if (pcsr.T == 0 && pcsr.PCS & 1)
	{
		static bool first = true;
		if (first)
		{
			first = false;
			_DBGPRT("[!] IMPLEMENTATION DEFINED format is found 0x%08x\n", pcsr.raw);
		}
	}

	if (offset)
	{
		if (pcsr.T == 0)
			*pc = ((pcsr.PCS << 1) & 0xFFFFFFFC) - 8;
		else
			*pc = (pcsr.PCS << 1) - 4;
	}
	else
	{
		if (pcsr.T == 0)
			*pc = (pcsr.PCS << 1) & 0xFFFFFFFC;
		else
			*pc = (pcsr.PCS << 1);
	}
	return OK;
}

errno_t ARMv7ARDIF::getCIDSR(uint32_t* cid)
{
	if (cid == nullptr)
		return EINVAL;

	errno_t ret = ap.read(REG_DBGCIDSR, cid);
	if (ret != OK)
		return ret;

	return OK;
}

void ARMv7ARDIF::printDSCR()
{
	DBGDSCR dscr;
	errno_t ret = readDSCR(&dscr);
	if (ret != OK)
	{
		_DBGPRT("  Failed to read DSCR. (0x%08x)\n", ret);
		return;
	}
	dscr.print();
}

void ARMv7ARDIF::printPRSR()
{
	uint32_t prsr;
	errno_t ret = ap.read(REG_DBGPRSR, &prsr);
	if (ret != OK)
	{
		_DBGPRT("  Failed to get PRSR(0x%08x)\n", ret);
		return;
	}

	_DBGPRT("  PRSR(powerdown and reset status): 0x%08x\n", prsr);
}

errno_t ARMv7ARDIF::readDSCR(DBGDSCR* dscr)
{
	if (dscr == nullptr)
		return EINVAL;

	errno_t ret = ap.read(REG_DBGDSCR, &dscr->raw);
	if (ret != OK)
		return ret;

	return OK;
}

errno_t ARMv7ARDIF::halt()
{
	DBGDSCR dscr;
	errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
	if (ret != OK)
		return ret;

	if (dscr.HALTED)
		return OK;

	DBGDRCR drcr = { };
	drcr.HRQ = 1;
	ret = ap.write(REG_DBGDRCR, drcr.raw);
	if (ret != OK)
		return ret;

	uint32_t counter = 0;
	while (1)
	{
		errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
		if (ret != OK)
			return ret;

		if (dscr.HALTED)
		{
			if (dscr.ITRen == 0)
			{
				dscr.ITRen = 1;
				ret = ap.write(REG_DBGDSCR, dscr.raw);
				if (ret != OK)
				{
					_DBGPRT("Failed to set ITRen.\n");
					return ret;
				}
			}
			break;
		}

		counter++;
		if (counter >= 100)
		{
			_DBGPRT("Failed to halt. (DSCR: 0x%08x)\n", dscr.raw);
			dscr.printIfNotSame();
			return EFAULT;
		}
	}
	return OK;
}

errno_t ARMv7ARDIF::run()
{
	// TODO
	DBGDSCR dscr;
	errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
	if (ret != OK)
		return ret;

	if (dscr.HALTED == 0)
		return OK;

	if (dscr.ITRen)
	{
		dscr.ITRen = 0;
		ret = ap.write(REG_DBGDSCR, dscr.raw);
		if (ret != OK)
			return ret;
	}

	DBGDRCR drcr = { };
	drcr.RRQ = 1;
	ret = ap.write(REG_DBGDRCR, drcr.raw);
	if (ret != OK)
		return ret;

	uint32_t counter = 0;
	while (1)
	{
		errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
		if (ret != OK)
			return ret;

		if (dscr.HALTED == 0)
			break;

		counter++;
		if (counter >= 100)
		{
			_DBGPRT("Failed to restart. (DSCR: 0x%08x)\n", dscr.raw);
			dscr.printIfNotSame();
			return EFAULT;
		}
	}
	return OK;
}

errno_t ARMv7ARDIF::readDCC(uint32_t* val)
{
	if (val == nullptr)
		return EINVAL;

	uint32_t counter = 0;
	while (1)
	{
		DBGDSCR dscr;
		errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
		if (ret != OK)
			return ret;

		if (dscr.TXfull)
			break;

		counter++;
		if (counter >= 100)
		{
			_DBGPRT("DTR TX is not full. (DSCR: 0x%08x)\n", dscr.raw);
			dscr.printIfNotSame();
			return EFAULT;
		}
	}

	errno_t ret = ap.read(REG_DBGDTRTX, val);
	if (ret != OK)
		return ret;

	return OK;
}

errno_t ARMv7ARDIF::writeDCC(uint32_t val)
{
	uint32_t counter = 0;
	while (1)
	{
		DBGDSCR dscr;
		errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
		if (ret != OK)
			return ret;

		if (dscr.RXfull == 0)
			break;

		counter++;
		if (counter >= 100)
		{
			_DBGPRT("DTR RX is full. (DSCR: 0x%08x)\n", dscr.raw);
			dscr.printIfNotSame();
			return EFAULT;
		}
	}

	errno_t ret = ap.write(REG_DBGDTRRX, val);
	if (ret != OK)
		return ret;

	return OK;

}

errno_t ARMv7ARDIF::writeITR(uint32_t val)
{
	uint32_t counter = 0;
	while (1)
	{
		DBGDSCR dscr;
		errno_t ret = ap.read(REG_DBGDSCR, &dscr.raw);
		if (ret != OK)
			return ret;

		if (dscr.InstrCompl_l)
			break;

		counter++;
		if (counter >= 100)
		{
			_DBGPRT("InstrCompl_l is 0. (DSCR: 0x%08x)\n", dscr.raw);
			dscr.printIfNotSame();
			return EFAULT;
		}
	}

	errno_t ret = ap.write(REG_DBGITR, val);
	if (ret != OK)
		return ret;
	return OK;
}