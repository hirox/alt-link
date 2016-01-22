
#include "stdafx.h"
#include "ARMv6MSCS.h"

#include <thread>
#include <chrono>

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
		uint32_t C_STEP		: 1;
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
		uint32_t C_STEP		: 1;
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

int32_t ARMv6MSCS::readCPUID(CPUID* cpuid)
{
	int32_t ret;

	if (cpuid == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	ret = ap.read(REG_CPUID, &cpuid->raw);
	if (ret != OK)
		return ret;

	return OK;
}

void ARMv6MSCS::CPUID::print()
{
	_DBGPRT("    CPUID          : 0x%08x\n", raw);
	_DBGPRT("      Implementer  : %s\n",
		Implementer == 0x41 ? "ARM" :
		Implementer == 0x44 ? "DEC" :
		Implementer == 0x4D ? "Motorola/Freescale" :
		Implementer == 0x51 ? "QUALCOMM" :
		Implementer == 0x56 ? "Marvell" :
		Implementer == 0x69 ? "Intel" : "UNKNOWN");
	_DBGPRT("      Architecture : %s\n",
		Architecture == 0x01 ? "ARMv4" :
		Architecture == 0x02 ? "ARMv4T" :
		Architecture == 0x03 ? "ARMv5" :
		Architecture == 0x04 ? "ARMv5T" :
		Architecture == 0x05 ? "ARMv5TE" :
		Architecture == 0x06 ? "ARMv5TEJ" :
		Architecture == 0x07 ? "ARMv6" :
		Architecture == 0x0C ? "ARMv6-M" :
		Architecture == 0x0F ? "ARMv7" : "UNKNOWN");
	_DBGPRT("      Part number  : %s\n",
		PartNo == 0xC05 ? "Cortex-A5" :
		PartNo == 0xC07 ? "Cortex-A7" :
		PartNo == 0xC08 ? "Cortex-A8" :
		PartNo == 0xC09 ? "Cortex-A9" :
		PartNo == 0xC0D ? "Cortex-A12" :
		PartNo == 0xC0E ? "Cortex-A17" :
		PartNo == 0xC0F ? "Cortex-A15" :
		PartNo == 0xC14 ? "Cortex-R4" :
		PartNo == 0xC15 ? "Cortex-R5" :
		PartNo == 0xC17 ? "Cortex-R7" :
		PartNo == 0xC20 ? "Cortex-M0" :
		PartNo == 0xC21 ? "Cortex-M1" :
		PartNo == 0xC23 ? "Cortex-M3" :
		PartNo == 0xC24 ? "Cortex-M4" :
		PartNo == 0xC27 ? "Cortex-M7" :
		PartNo == 0xC60 ? "Cortex-M0+" : "UNKNOWN");
	_DBGPRT("      Revision     : r%xp%x\n", Variant, Revision);
}

int32_t ARMv6MSCS::readDFSR(DFSR* dfsr)
{
	int32_t ret;

	if (dfsr == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	ret = ap.read(REG_DFSR, &dfsr->raw);
	if (ret != OK)
		return ret;

	return OK;
}

void ARMv6MSCS::DFSR::print()
{
	_DBGPRT("    DFSR           : 0x%08x (%s%s%s%s%s%s)\n", raw,
		raw == 0 ? "Running" : "",
		EXTERNAL ? "EXTERNAL" : "",
		VCATCH ? "VectorCatch" : "",
		DWTTRAP ? "DWTTRAP" : "",
		BKPT ? "BKPT" : "",
		HALTED ? "/HALTED" : "");
}

int32_t ARMv6MSCS::waitForRegReady()
{
	int ret;

	while (1)
	{
		DHCSR_R d;
		ret = ap.read(REG_DHCSR, &d.raw);
		if (ret != OK)
			return ret;

		if (d.C_HALT == 0)
			return CMSISDAP_ERR_INVALID_STATUS;

		if (d.S_REGRDY == 1)
			break;

		// [TODO] timeout

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	return OK;
}

int32_t ARMv6MSCS::readReg(REGSEL reg, uint32_t* data)
{
	if (data == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	if (reg == 19 || reg > 20)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	DCRSR dcrsr;
	dcrsr.raw = 0;
	dcrsr.REGSEL = reg;

	int ret = ap.write(REG_DCRSR, dcrsr.raw);
	if (ret != OK)
		return ret;

	waitForRegReady();
	if (ret != OK)
		return ret;

	ret = ap.read(REG_DCRDR, data);
	if (ret != OK)
		return ret;

	//_DBGPRT("readReg %d 0x%08x\n", reg, *data);
	return OK;
}

int32_t ARMv6MSCS::writeReg(REGSEL reg, uint32_t data)
{
	if (reg == 19 || reg > 20)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	int ret = ap.write(REG_DCRDR, data);
	if (ret != OK)
		return ret;

	DCRSR dcrsr;
	dcrsr.raw = 0;
	dcrsr.REGSEL = reg;
	dcrsr.REGWnR = 1;

	ret = ap.write(REG_DCRSR, dcrsr.raw);
	if (ret != OK)
		return ret;

	ret = waitForRegReady();
	if (ret != OK)
		return ret;

	return OK;
}

void ARMv6MSCS::printRegs()
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

void ARMv6MSCS::printDHCSR()
{
	DHCSR_R d;
	if (ap.read(REG_DHCSR, &d.raw) != OK)
		return;

	_DBGPRT("    DHCSR          : 0x%08x\n", d.raw);
	_DBGPRT("      C_DEBUGEN  : %x C_HALT    : %x C_STEP : %x C_MASKINTS: %x\n"
		, d.C_DEBUGEN, d.C_HALT, d.C_STEP, d.C_MASKINTS);
	_DBGPRT("      S_REGRDY   : %x S_HALT    : %x S_SLEEP: %x S_LOCKUP  : %x\n"
		, d.S_REGRDY, d.S_HALT, d.S_SLEEP, d.S_LOCKUP);
	_DBGPRT("      S_RETIRE_ST: %x S_RESET_ST: %x\n"
		, d.S_RETIRE_ST, d.S_RESET_ST);
}

void ARMv6MSCS::printDEMCR()
{
	DEMCR d;
	if (ap.read(REG_DEMCR, &d.raw) != OK)
		return;

	_DBGPRT("    DEMCR         : 0x%08x\n", d.raw);
	_DBGPRT("      DWT         : %s\n", d.DWTENA ? "enabled" : "disabled");
	_DBGPRT("      HardFault   : %s\n", d.VC_HARDERR ? "tarp" : "don't trap");
	_DBGPRT("      ResetVector : %s\n", d.VC_CORERESET ? "trap" : "don't trap");
}

errno_t ARMv6MSCS::isHalt(bool* halt)
{
	ASSERT_RELEASE(halt != nullptr);

	int32_t ret;
	DHCSR_R d;
	ret = ap.read(REG_DHCSR, &d.raw);
	if (ret != OK)
		return ret;

	*halt = d.S_HALT ? true : false;
	return OK;
}

int32_t ARMv6MSCS::halt(bool maskIntr)
{
	int32_t ret;
	DHCSR_W d;
	d.raw = 0;

	d.DBGKEY = 0xA05F;
	d.C_DEBUGEN = 1;
	d.C_HALT = 1;
	d.C_STEP = 0;
	d.C_MASKINTS = maskIntr ? 1 : 0;

	ret = ap.write(REG_DHCSR, d.raw);
	if (ret != OK)
		return ret;

	_DBGPRT("halt success\n");
	return OK;
}

int32_t ARMv6MSCS::run(bool maskIntr)
{
	int32_t ret;
	DHCSR_W d;
	ret = ap.read(REG_DHCSR, &d.raw);
	if (ret != OK)
		return ret;

	uint32_t mask = maskIntr ? 1 : 0;
	if (mask != d.C_MASKINTS)
	{
		ret = halt(maskIntr);
		if (ret != OK)
			return ret;
	}

	d.DBGKEY = 0xA05F;
	d.C_DEBUGEN = 1;
	d.C_HALT = 0;
	d.C_STEP = 0;
	d.C_MASKINTS = maskIntr ? 1 : 0;

	ret = ap.write(REG_DHCSR, d.raw);
	if (ret != OK)
		return ret;

	_DBGPRT("run success\n");
	return OK;
}

int32_t ARMv6MSCS::step(bool maskIntr)
{
	int32_t ret;
	DHCSR_R r;
	ret = ap.read(REG_DHCSR, &r.raw);
	if (ret != OK)
		return ret;

	if (!r.C_DEBUGEN || !r.S_HALT)
	{
		ret = halt();
		if (ret != OK)
			return ret;
	}

	DHCSR_W w;
	w.raw = 0;
	w.DBGKEY = 0xA05F;
	w.C_DEBUGEN = 1;
	w.C_HALT = 0;
	w.C_STEP = 1;
	w.C_MASKINTS = maskIntr ? 1 : 0;

	ret = ap.write(REG_DHCSR, w.raw);
	if (ret != OK)
		return ret;

	_DBGPRT("step success\n");
	return OK;
}