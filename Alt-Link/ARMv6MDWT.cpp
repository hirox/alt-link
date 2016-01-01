
#include "stdafx.h"
#include "ADIv5.h"

#define _DBGPRT printf

// v6-M, v7-M
#define REG_DWT_CTRL		(base + 0x000)
#define REG_DWT_PCSR		(base + 0x01C)
#define REG_DWT_COMP0		(base + 0x020)
#define REG_DWT_MASK0		(base + 0x024)
#define REG_DWT_FUNCTION0	(base + 0x028)

// v7-M
#define REG_DWT_CYCCNT		(base + 0x004)
#define REG_DWT_CPICNT		(base + 0x008)
#define REG_DWT_EXCCNT		(base + 0x00C)
#define REG_DWT_SLEEPCNT	(base + 0x010)
#define REG_DWT_LSUCNT		(base + 0x014)
#define REG_DWT_FOLDCNT		(base + 0x018)
#define REG_DWT_LSR			(base + 0xFB4)

union DWT_CTRL_V6M
{
	struct
	{
		uint32_t Reserved		: 28;
		uint32_t NUMCOMP		: 4;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DWT_CTRL_V6M));

union DWT_CTRL_V7M
{
	struct
	{
		uint32_t CYCCNTENA		: 1;
		uint32_t POSTPRESET		: 4;
		uint32_t POSTINIT		: 4;
		uint32_t CYCTAP			: 1;
		uint32_t SYNCTAP		: 2;
		uint32_t PCSAMPLENA		: 1;
		uint32_t Reserved0		: 3;
		uint32_t EXCTRCENA		: 1;
		uint32_t CPIEVTENA		: 1;
		uint32_t EXCEVTENA		: 1;
		uint32_t SLEEPEVTENA	: 1;
		uint32_t LSUEVTENA		: 1;
		uint32_t FOLDEVTENA		: 1;
		uint32_t CYCEVTENA		: 1;
		uint32_t Reserved1		: 1;
		uint32_t NOPRFCNT		: 1;
		uint32_t NOCYCCNT		: 1;
		uint32_t NOEXTTRIG		: 1;
		uint32_t NOTRCPKT		: 1;
		uint32_t NUMCOMP		: 4;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DWT_CTRL_V7M));

int32_t ADIv5::ARMv6MDWT::getPC(uint32_t* pc)
{
	if (pc == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	int ret = ap.read(REG_DWT_PCSR, pc);
	if (ret != OK)
		return ret;

	return OK;
}

void ADIv5::ARMv6MDWT::printPC()
{
	uint32_t pc;
	int ret = getPC(&pc);
	if (ret != OK)
		return;

	_DBGPRT("    PC : 0x%08x\n", pc);
}

void ADIv5::ARMv6MDWT::printCtrl()
{
	DWT_CTRL_V6M data;
	int ret = ap.read(REG_DWT_CTRL, &data.raw);
	if (ret != OK)
		return;

	_DBGPRT("    DWT_CTRL : 0x%08x\n", data.raw);
	_DBGPRT("      NUMCOMP: %x\n", data.NUMCOMP);
}

void ADIv5::ARMv7MDWT::printCtrl()
{
	DWT_CTRL_V7M data;
	int ret = ap.read(REG_DWT_CTRL, &data.raw);
	if (ret != OK)
		return;

	_DBGPRT("    DWT_CTRL     : 0x%08x\n", data.raw);
	_DBGPRT("      NUMCOMP    : %x NOTRCPKT : %x NOEXTTRIG : %x NOCYCCNT : %x\n",
		data.NUMCOMP, data.NOTRCPKT, data.NOEXTTRIG, data.NOCYCCNT);
	_DBGPRT("      NOPRFCNT   : %x CYCEVTENA: %x FOLDEVTENA: %x LSUEVTENA: %x\n",
		data.NOPRFCNT, data.CYCEVTENA, data.FOLDEVTENA, data.LSUEVTENA);
	_DBGPRT("      SLEEPEVTENA: %x EXCEVTENA: %x CPIEVTENA : %x EXCTRCENA: %x\n",
		data.SLEEPEVTENA, data.EXCEVTENA, data.CPIEVTENA, data.EXCTRCENA);
	_DBGPRT("      PCSAMPLENA : %x SYNCTAP  : %x CYCTAP    : %x POSTINIT : %x\n",
		data.PCSAMPLENA, data.SYNCTAP, data.CYCTAP, data.POSTINIT);
	_DBGPRT("      POSTPRESET : %x CYCCNTENA: %x\n",
		data.POSTPRESET, data.CYCCNTENA);
}
