
#include "stdafx.h"
#include "ARMv7MFPB.h"

// v7-M
#define REG_FP_CTRL			(base + 0x000)
#define REG_FP_REMAP		(base + 0x004)
#define REG_FP_COMP0		(base + 0x008)
#define REG_FP_COMP1		(base + 0x00C)
#define REG_FP_COMP2		(base + 0x010)
#define REG_FP_COMP3		(base + 0x014)
#define REG_FP_COMP4		(base + 0x018)
#define REG_FP_COMP5		(base + 0x01C)
#define REG_FP_COMP6		(base + 0x020)
#define REG_FP_COMP7		(base + 0x024)
#define REG_FP_LAR			(base + 0xFB0)
#define REG_FP_LSR			(base + 0xFB4)

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
};
static_assert(CONFIRM_UINT32(FP_COMP));

errno_t ARMv7MFPB::init()
{
	errno_t ret = ap.read(REG_FP_CTRL, &ctrl.raw);
	if (ret != OK)
		return ret;

	initialized = true;
	return OK;
}

bool ARMv7MFPB::isEnabled()
{
	if (!initialized)
		return false;

	return ctrl.ENABLE ? true : false;
}

errno_t ARMv7MFPB::setEnable(bool enable)
{
	if (!initialized)
		return EPERM;

	FP_CTRL _ctrl = ctrl;
	_ctrl.ENABLE = enable ? 1 : 0;
	_ctrl.KEY = 1;

	errno_t ret = ap.write(REG_FP_CTRL, _ctrl.raw);
	if (ret != OK)
		return ret;

	ctrl = _ctrl;
	return OK;
}

errno_t ARMv7MFPB::setBreakPoint(bool enable, uint32_t index, uint32_t addr)
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

	FP_COMP comp;
	comp.raw = 0;
	comp.REPLACE = enable ? ((addr & 0x2) != 0 ? FP_COMP::UPPER_HALF : FP_COMP::LOWER_HALF) : FP_COMP::REMAP;
	comp.ENABLE = enable ? 1 : 0;
	comp.COMP = (addr & 0x1FFFFFFC) >> 2;
	return ap.write(REG_FP_COMP0 + (index * 4), comp.raw);
}

void ARMv7MFPB::printCtrl()
{
	if (!initialized)
		return;

	_DBGPRT("    FP_CTRL      : 0x%08x\n", ctrl.raw);
	_DBGPRT("      ENABLE     : %x KEY : %x\n",
		ctrl.ENABLE, ctrl.KEY);
	_DBGPRT("      NUM_CODE   : %x NUM_LIT: %x\n",
		ctrl.num(), ctrl.NUM_LIT);
}

void ARMv7MFPB::printRemap()
{
	FP_REMAP data;
	int ret = ap.read(REG_FP_REMAP, &data.raw);
	if (ret != OK)
		return;

	_DBGPRT("    FP_REMAP     : 0x%08x\n", data.raw);
	_DBGPRT("      REMAP      : 0x%08x (%s)\n",
		0x20000000 + (data.REMAP << 5), data.RMPSPT ? "Supported" : "Not Supported");
}
