
#pragma once

#include <cstdint>
#include "ADIv5.h"

union BP_CTRL
{
	struct
	{
		uint32_t ENABLE			: 1;
		uint32_t KEY			: 1;
		uint32_t Reserved0		: 2;
		uint32_t NUM_CODE		: 4;
		uint32_t Reserved1		: 24;
	};
	uint32_t raw;

	uint32_t num() { return NUM_CODE; }
};
static_assert(CONFIRM_UINT32(BP_CTRL));

class ARMv6MBPU : public ADIv5::Memory
{
private:
	bool initialized;
	BP_CTRL ctrl;

public:
	ARMv6MBPU(const Memory& memory) : Memory(memory), initialized(false) {}

	errno_t init();
	bool isEnabled();
	errno_t setEnable(bool enable = true);
	errno_t setBreakPoint(bool enable, uint32_t index, uint32_t addr);

	void printCtrl();
};
