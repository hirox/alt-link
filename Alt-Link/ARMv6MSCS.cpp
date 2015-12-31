
#include "stdafx.h"
#include "common.h"

#include "CMSIS-DAP.h"

#define _DBGPRT printf

#define REG_ACTLR	(base + 0x008)
#define REG_CPUID	(base + 0xD00)
#define REG_ICSR	(base + 0xD04)
#define REG_VTOR	(base + 0xD08)
#define REG_AIRCR	(base + 0xD0C)
#define REG_SCR		(base + 0xD10)
#define REG_CCR		(base + 0xD14)
#define REG_SHPR2	(base + 0xD1C)
#define REG_SHPR3	(base + 0xD20)
#define REG_SHCSR	(base + 0xD24)
#define REG_DFSR	(base + 0xD30)
#define REG_DHCSR	(base + 0xDF0)
#define REG_DCRSR	(base + 0xDF4)
#define REG_DCRDR	(base + 0xDF8)
#define REG_DEMCR	(base + 0xDFC)

union DHCSR_R
{
	struct
	{
		uint32_t C_DEBUGEN	: 1;
		uint32_t C_HALT		: 1;
		uint32_t C_STP		: 1;
		uint32_t C_MASKINTS	: 1;
		uint32_t Reserved0	: 12;

		uint32_t S_REGRDY		: 1;
		uint32_t S_HALT			: 1;
		uint32_t S_SLEEP		: 1;
		uint32_t S_LOCKUP		: 1;
		uint32_t Reserved1		: 4;
		uint32_t S_RETIRE_ST	: 1;
		uint32_t S_RESET_ST		: 1;
		uint32_t Reserved2		: 6;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DHCSR_R));

union DHCSR_W
{
	struct
	{
		uint32_t C_DEBUGEN	: 1;
		uint32_t C_HALT		: 1;
		uint32_t C_STP		: 1;
		uint32_t C_MASKINTS	: 1;
		uint32_t Reserved0	: 12;

		uint32_t DBGKEY		: 16;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DHCSR_W));

// Debug Core Register Selector Register
//	accessible only in Debug state
union DCRSR
{
	struct
	{
		uint32_t REGSEL		: 5;
		uint32_t Reserved0	: 11;
		uint32_t REGWnR		: 1;
		uint32_t Reserved1	: 15;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DCRSR));

union DEMCR
{
	struct
	{
		uint32_t VC_CORERESET	: 1;
		uint32_t Reserved0		: 9;
		uint32_t VC_HARDERR		: 1;
		uint32_t Reserved1		: 13;
		uint32_t DWTENA			: 1;
		uint32_t Reserved2		: 7;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DEMCR));

int32_t CMSISDAP::ARMv6MSCS::readCPUID(CPUID* cpuid)
{
	int32_t ret;

	if (cpuid == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	ret = ap.read(REG_CPUID, &cpuid->raw);
	if (ret != CMSISDAP_OK)
		return ret;

	return CMSISDAP_OK;
}

void CMSISDAP::ARMv6MSCS::printCPUID(const CPUID& cpuid)
{
	_DBGPRT("    CPUID          : 0x%08x\n", cpuid.raw);
	_DBGPRT("      Implementer  : %s\n",
		cpuid.Implementer == 0x41 ? "ARM" :
		cpuid.Implementer == 0x44 ? "DEC" :
		cpuid.Implementer == 0x4D ? "Motorola/Freescale" :
		cpuid.Implementer == 0x51 ? "QUALCOMM" :
		cpuid.Implementer == 0x56 ? "Marvell" :
		cpuid.Implementer == 0x69 ? "Intel" : "UNKNOWN");
	_DBGPRT("      Architecture : %s\n",
		cpuid.Architecture == 0x01 ? "ARMv4" :
		cpuid.Architecture == 0x02 ? "ARMv4T" :
		cpuid.Architecture == 0x03 ? "ARMv5" :
		cpuid.Architecture == 0x04 ? "ARMv5T" :
		cpuid.Architecture == 0x05 ? "ARMv5TE" :
		cpuid.Architecture == 0x06 ? "ARMv5TEJ" :
		cpuid.Architecture == 0x07 ? "ARMv6" :
		cpuid.Architecture == 0x0C ? "ARMv6-M" :
		cpuid.Architecture == 0x0F ? "ARMv7" : "UNKNOWN");
	_DBGPRT("      Part number  : %s\n",
		cpuid.PartNo == 0xC05 ? "Cortex-A5" :
		cpuid.PartNo == 0xC07 ? "Cortex-A7" :
		cpuid.PartNo == 0xC08 ? "Cortex-A8" :
		cpuid.PartNo == 0xC09 ? "Cortex-A9" :
		cpuid.PartNo == 0xC0D ? "Cortex-A12" :
		cpuid.PartNo == 0xC0E ? "Cortex-A17" :
		cpuid.PartNo == 0xC0F ? "Cortex-A15" :
		cpuid.PartNo == 0xC14 ? "Cortex-R4" :
		cpuid.PartNo == 0xC15 ? "Cortex-R5" :
		cpuid.PartNo == 0xC17 ? "Cortex-R7" :
		cpuid.PartNo == 0xC20 ? "Cortex-M0" :
		cpuid.PartNo == 0xC21 ? "Cortex-M1" :
		cpuid.PartNo == 0xC23 ? "Cortex-M3" :
		cpuid.PartNo == 0xC24 ? "Cortex-M4" :
		cpuid.PartNo == 0xC27 ? "Cortex-M7" :
		cpuid.PartNo == 0xC60 ? "Cortex-M0+" : "UNKNOWN");
	_DBGPRT("      Revision     : r%xp%x\n", cpuid.Variant, cpuid.Revision);
}

int32_t CMSISDAP::ARMv6MSCS::readDFSR(DFSR* dfsr)
{
	int32_t ret;

	if (dfsr == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	ret = ap.read(REG_DFSR, &dfsr->raw);
	if (ret != CMSISDAP_OK)
		return ret;

	return CMSISDAP_OK;
}

void CMSISDAP::ARMv6MSCS::printDFSR(const DFSR& dfsr)
{
	_DBGPRT("    DFSR           : 0x%08x (%s%s%s%s%s%s)\n", dfsr.raw,
		dfsr.raw == 0 ? "Running" : "",
		dfsr.EXTERNAL ? "EXTERNAL" : "",
		dfsr.VCATCH ? "VectorCatch" : "",
		dfsr.DWTTRAP ? "DWTTRAP" : "",
		dfsr.BKPT ? "BKPT" : "",
		dfsr.HALTED ? "HALTED" : "");
}

int32_t CMSISDAP::ARMv6MSCS::readReg(REGSEL reg, uint32_t* data)
{
	if (data == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	if (reg == 19 || reg > 20)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	DCRSR dcrsr;
	dcrsr.raw = 0;
	dcrsr.REGSEL = reg;

	int32_t ret = ap.write(0xE000EDF4, dcrsr.raw);
	if (ret != CMSISDAP_OK)
		return ret;

	ret = ap.read(0xE000EDF8, data);
	if (ret != CMSISDAP_OK)
		return ret;

	return CMSISDAP_OK;
}

int32_t CMSISDAP::ARMv6MSCS::writeReg(REGSEL reg, uint32_t data)
{
	if (reg == 19 || reg > 20)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	int32_t ret = ap.write(0xE000EDF8, data);
	if (ret != CMSISDAP_OK)
		return ret;

	DCRSR dcrsr;
	dcrsr.raw = 0;
	dcrsr.REGSEL = reg;
	dcrsr.REGWnR = 1;

	ret = ap.write(0xE000EDF4, dcrsr.raw);
	if (ret != CMSISDAP_OK)
		return ret;

	return CMSISDAP_OK;
}

void CMSISDAP::ARMv6MSCS::printRegs()
{
	uint32_t data[4];

	_DBGPRT("    Registers\n");

	readReg(R0, &data[0]);
	readReg(R1, &data[1]);
	readReg(R2, &data[2]);
	readReg(R3, &data[3]);
	_DBGPRT("      R0-R3 : 0x%08x 0x%08x 0x%08x 0x%08x\n"
		, data[0], data[1], data[2], data[3]);

	readReg(R4, &data[0]);
	readReg(R5, &data[1]);
	readReg(R6, &data[2]);
	readReg(R7, &data[3]);
	_DBGPRT("      R4-R7 : 0x%08x 0x%08x 0x%08x 0x%08x\n"
		, data[0], data[1], data[2], data[3]);

	readReg(R8,  &data[0]);
	readReg(R9,  &data[1]);
	readReg(R10, &data[2]);
	readReg(R11, &data[3]);
	_DBGPRT("      R8-R11: 0x%08x 0x%08x 0x%08x 0x%08x\n"
		, data[0], data[1], data[2], data[3]);

	readReg(R12, &data[0]);
	readReg(SP,  &data[1]);
	readReg(LR,  &data[2]);
	readReg(DebugReturnAddress, &data[3]);
	_DBGPRT("      R12   : 0x%08x SP : 0x%08x LR : 0x%08x PC: 0x%08x\n"
		, data[0], data[1] , data[2], data[3]);

	readReg(xPSR, &data[0]);
	readReg(MSP, &data[1]);
	readReg(PSP, &data[2]);
	readReg(CONTROL_PRIMASK, &data[3]);
	_DBGPRT("      xPSR  : 0x%08x MSP: 0x%08x PSP: 0x%08x CPM: 0x%08x\n"
		, data[0], data[1], data[2], data[3]);
}

