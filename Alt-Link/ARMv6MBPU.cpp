
#include "stdafx.h"
#include "ARMv6MBPU.h"

// v6-M
#define REG_BP_CTRL			(base + 0x000)
#define REG_BP_COMP0		(base + 0x008)
#define REG_BP_COMP1		(base + 0x008)
#define REG_BP_COMP2		(base + 0x008)
#define REG_BP_COMP3		(base + 0x008)

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
};
static_assert(CONFIRM_UINT32(BP_COMP));

errno_t ARMv6MBPU::init()
{
	errno_t ret = ap.read(REG_BP_CTRL, &ctrl.raw);
	if (ret != OK)
		return ret;

	initialized = true;
	return OK;
}

bool ARMv6MBPU::isEnabled()
{
	if (!initialized)
		return false;

	return ctrl.ENABLE ? true : false;
}

errno_t ARMv6MBPU::setEnable(bool enable)
{
	if (!initialized)
		return EPERM;

	BP_CTRL _ctrl = ctrl;
	_ctrl.ENABLE = enable ? 1 : 0;
	_ctrl.KEY = 1;

	errno_t ret = ap.write(REG_BP_CTRL, _ctrl.raw);
	if (ret != OK)
		return ret;

	ctrl = _ctrl;
	return OK;
}

errno_t ARMv6MBPU::setBreakPoint(bool enable, uint32_t index, uint32_t addr)
{
	if (!initialized)
		return EPERM;

	// Code: 0x00000000 - 0x20000000
	if ((addr & 0x1) != 0 || (addr & 0xE0000000) != 0)
		return EINVAL;

	if (index >= ctrl.num())
		return EINVAL;

	if (!isEnabled())
	{
		errno_t ret = setEnable();
		if (ret != OK)
			return ret;
	}

	BP_COMP comp;
	comp.raw = 0;
	comp.BP_MATCH = enable ? ((addr & 0x2) != 0 ? BP_COMP::UPPER_HALF : BP_COMP::LOWER_HALF) : BP_COMP::NO_BP;
	comp.ENABLE = enable ? 1 : 0;
	comp.COMP = (addr & 0x1FFFFFFC) >> 2;
	return ap.write(REG_BP_COMP0 + (index * 4), comp.raw);
}

void ARMv6MBPU::printCtrl()
{
	if (!initialized)
		return;

	_DBGPRT("    FP_CTRL      : 0x%08x\n", ctrl.raw);
	_DBGPRT("      ENABLE     : %x KEY : %x\n",
		ctrl.ENABLE, ctrl.KEY);
	_DBGPRT("      NUM_CODE   : %x\n",
		ctrl.num());
}
