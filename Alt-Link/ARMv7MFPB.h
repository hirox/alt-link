
#pragma once

#include <cstdint>
#include "ADIv5.h"

union FP_CTRL
{
	struct
	{
		uint32_t ENABLE			: 1;
		uint32_t KEY			: 1;
		uint32_t Reserved0		: 2;
		uint32_t NUM_CODE0		: 4;
		uint32_t NUM_LIT		: 4;
		uint32_t NUM_CODE1		: 3;
		uint32_t Reserved		: 17;
	};
	uint32_t raw;

	uint32_t num() { return NUM_CODE0 + (NUM_CODE1 << 4); }
};
static_assert(CONFIRM_UINT32(FP_CTRL));

union FP_REMAP
{
	struct
	{
		uint32_t Reserved0		: 5;
		uint32_t REMAP			: 24;
		uint32_t RMPSPT			: 1;
		uint32_t Reserved1		: 2;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(FP_REMAP));

class ARMv7MFPB : public ADIv5::Memory
{
private:
	bool initialized;
	FP_CTRL ctrl;
	FP_REMAP remap;

public:
	ARMv7MFPB(const Memory& memory) : Memory(memory), initialized(false) {}

	errno_t init();
	bool isEnabled();
	errno_t setEnable(bool enable = true);
	errno_t setBreakPoint(bool enable, uint32_t index, uint32_t addr);

	void printCtrl();
	void printRemap();
};
