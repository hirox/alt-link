
#include "stdafx.h"
#include "common.h"

#include "CMSIS-DAP.h"

void CMSISDAP::setTargetThreadId()
{
	// TODO
}

void CMSISDAP::setCurrentPC(const uint64_t addr)
{
	// TODO
	(void)addr;
}

void CMSISDAP::resume()
{
	// TODO
	// continue command
}

int32_t CMSISDAP::step(uint8_t* signal)
{
	ASSERT_RELEASE(signal != nullptr);

	*signal = 0x05;	// SIGTRAP
	return 0;
}

int32_t CMSISDAP::interrupt(uint8_t* signal)
{
	ASSERT_RELEASE(signal != nullptr);

	*signal = 0x05;	// SIGTRAP
	return 0;
}

int32_t CMSISDAP::setBreakPoint(BreakPointType type, uint64_t addr, uint32_t kind)
{
	return -1;	// not supported
}

int32_t CMSISDAP::unsetBreakPoint(BreakPointType type, uint64_t addr, uint32_t kind)
{
	return -1;	// not supported
}

int32_t CMSISDAP::setWatchPoint(WatchPointType type, uint64_t addr, uint32_t kind)
{
	return -1;	// not supported
}

int32_t CMSISDAP::unsetWatchPoint(WatchPointType type, uint64_t addr, uint32_t kind)
{
	return -1;	// not supported
}

int32_t CMSISDAP::readRegister(const uint32_t n, uint32_t* out)
{
	ASSERT_RELEASE(out != nullptr);
	*out = 0xDEADBEEF;	// dummy
	return 0;
}

int32_t CMSISDAP::readRegister(const uint32_t n, uint64_t* out)
{
	ASSERT_RELEASE(out != nullptr);
	uint32_t value;
	int32_t result = readRegister(n, &value);
	if (result == OK)
	{
		*out = value;
	}
	return result;
}

int32_t CMSISDAP::readRegister(const uint32_t n, uint64_t* out1, uint64_t* out2)
{
	ASSERT_RELEASE(out1 != nullptr && out2 != nullptr);
	uint32_t value;
	int32_t result = readRegister(n, &value);
	if (result == OK)
	{
		*out1 = value;
		*out2 = 0;
	}
	return result;
}

int32_t CMSISDAP::writeRegister(const uint32_t n, const uint32_t data)
{
	// TODO
	(void)n;
	(void)data;
	return 0;
}

int32_t CMSISDAP::writeRegister(const uint32_t n, const uint64_t data)
{
	(void)n;
	(void)data;
	return 0;
}

int32_t CMSISDAP::writeRegister(const uint32_t n, const uint64_t data1, const uint64_t data2)
{
	(void)n;
	(void)data1;
	(void)data2;
	return 0;
}

int32_t CMSISDAP::readGenericRegisters(std::vector<uint32_t>* array)
{
	ASSERT_RELEASE(array != nullptr);

	for (int i = 0; i < 16; i++)
	{
		uint32_t value;
		if (readRegister(i, &value) == OK)
		{
			array->push_back(value);
		}
		else
		{
			return -1;
		}
	}
	return 0;
}

int32_t CMSISDAP::writeGenericRegisters(const std::vector<uint32_t>& array)
{
	// TODO
	(void)array;
	return 0;
}

void CMSISDAP::readMemory(uint64_t addr, uint32_t len, std::vector<uint8_t>* array)
{
	ASSERT_RELEASE(array != nullptr);

	(void)addr;

	for (uint32_t i = 0; i < len; i++)
	{
		array->push_back(0xEF);	// dummy
	}
}

uint8_t CMSISDAP::writeMemory(uint64_t addr, uint32_t len, const std::vector<uint8_t>& array)
{
	// TODO
	(void)addr;
	(void)len;
	(void)array;
	return 0;
}