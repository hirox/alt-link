
#include "stdafx.h"
#include "ARMv7ARDIF.h"

#define _DBGPRT printf

#define REG_DBGDIDR		(base + 0x000)
#define REG_DBGDSCR		(base + 0x088)
#define REG_DBGPCSR_33	(base + 0x084)
#define REG_DBGPCSR_40	(base + 0x0A0)
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
		uint32_t MODE			: 4;
		uint32_t SDABORT_1		: 1;
		uint32_t ADABORT_1		: 1;
		uint32_t UND_1			: 1;
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
		uint32_t InstrComp1_1	: 1;
		uint32_t PipeAdv		: 1;
		uint32_t TXfull_1		: 1;
		uint32_t RXfull_1		: 1;
		uint32_t ZERO_1			: 1;
		uint32_t TXfull			: 1;
		uint32_t RXfull			: 1;
		uint32_t ZERO_2			: 1;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DBGDSCR));

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

errno_t ARMv7ARDIF::getPC(uint32_t *pc)
{
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

	// try to read from reg44
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

	// try to read from reg33 if reg44 is not found
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