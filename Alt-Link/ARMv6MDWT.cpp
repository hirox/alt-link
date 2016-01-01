
#include "stdafx.h"

#define _DBGPRT printf

#define REG_DWT_CTRL		(base + 0x000)
#define REG_DWT_CYCCNT		(base + 0x004)
#define REG_DWT_CPICNT		(base + 0x008)
#define REG_DWT_EXCCNT		(base + 0x00C)
#define REG_DWT_SLEEPCNT	(base + 0x010)
#define REG_DWT_LSUCNT		(base + 0x014)
#define REG_DWT_FOLDCNT		(base + 0x018)
#define REG_DWT_PCSR		(base + 0x01C)
#define REG_DWT_COMP0		(base + 0x020)
#define REG_DWT_MASK0		(base + 0x024)
#define REG_DWT_FUNCTION0	(base + 0x028)
#define REG_DWT_LSR			(base + 0xFB4)

int32_t CMSISDAP::ARMv6MDWT::getPC(uint32_t* pc)
{
	if (pc == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	int ret = ap.read(REG_DWT_PCSR, pc);
	if (ret != CMSISDAP_OK)
		return ret;

	return CMSISDAP_OK;
}

void CMSISDAP::ARMv6MDWT::printPC()
{
	uint32_t pc;
	int ret = getPC(&pc);
	if (ret != CMSISDAP_OK)
		return;

	_DBGPRT("    PC : 0x%08x\n", pc);
}

