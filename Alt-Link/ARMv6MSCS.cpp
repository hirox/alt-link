
#include "stdafx.h"
#include "common.h"

#include "CMSIS-DAP.h"

#define _DBGPRT printf

int32_t CMSISDAP::ARMv6MSCS::readCPUID(CPUID* cpuid)
{
	int32_t ret;

	if (cpuid == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	ret = ap.read(0xE000ED00, &cpuid->raw);
	if (ret != CMSISDAP_OK)
		return ret;

	return CMSISDAP_OK;
}

int32_t CMSISDAP::ARMv6MSCS::readDFSR(DFSR* dfsr)
{
	int32_t ret;

	if (dfsr == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	ret = ap.read(0xE000ED30, &dfsr->raw);
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
