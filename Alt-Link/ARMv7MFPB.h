
#pragma once

#include <cstdint>
#include "ADIv5.h"

class ARMv7MFPB : public ADIv5::Memory
{
private:
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

	union FP_COMP
	{
		enum TYPE {
			REMAP		= 0,
			LOWER_HALF	= 1,
			UPPER_HALF	= 2,
			BOTH_HALF	= 3
		};

		struct
		{
			uint32_t ENABLE			: 1;
			uint32_t Reserved0		: 1;
			uint32_t COMP			: 27;
			uint32_t Reserved1		: 1;
			TYPE REPLACE			: 2;
		};
		uint32_t raw;

		bool match(uint32_t addr) {
			if ((addr & 0x1FFFFFFC) >> 2 == COMP &&
				REPLACE == ((addr & 0x2) != 0 ? UPPER_HALF : LOWER_HALF))
				return true;
			return false;
		}
	};
	static_assert(CONFIRM_UINT32(FP_COMP));

private:
	bool initialized;
	FP_CTRL ctrl;
	FP_REMAP remap;
	std::vector<FP_COMP> bpList;

	int32_t findBreakPoint(uint32_t addr);
	int32_t findEmpty();

public:
	ARMv7MFPB(const Memory& memory) : Memory(memory), initialized(false) {}

	errno_t init();
	bool isEnabled();
	errno_t setEnable(bool enable = true);
	errno_t addBreakPoint(uint32_t addr);
	errno_t delBreakPoint(uint32_t addr);
	errno_t setBreakPoint(bool enable, uint32_t index, uint32_t addr);

	void printCtrl();
	void printRemap();
};
