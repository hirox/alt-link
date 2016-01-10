
#pragma once

#include <cstdint>
#include <vector>
#include "ADIv5.h"

class ARMv6MBPU : public ADIv5::Memory
{
private:
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

	union BP_COMP
	{
		enum TYPE {
			NO_BP		= 0,
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
			TYPE BP_MATCH			: 2;
		};
		uint32_t raw;

		bool match(uint32_t addr) {
			if ((addr & 0x1FFFFFFC) >> 2 == COMP &&
				BP_MATCH == ((addr & 0x2) != 0 ? UPPER_HALF : LOWER_HALF))
				return true;
			return false;
		}
	};
	static_assert(CONFIRM_UINT32(BP_COMP));

private:
	bool initialized;
	BP_CTRL ctrl;
	std::vector<BP_COMP> bpList;

	int32_t findBreakPoint(uint32_t addr);
	int32_t findEmpty();

public:
	ARMv6MBPU(const Memory& memory) : Memory(memory), initialized(false) {}

	errno_t init();
	bool isEnabled();
	errno_t setEnable(bool enable = true);
	errno_t addBreakPoint(uint32_t addr);
	errno_t delBreakPoint(uint32_t addr);
	errno_t setBreakPoint(bool enable, uint32_t index, uint32_t addr);

	void printCtrl();
};
