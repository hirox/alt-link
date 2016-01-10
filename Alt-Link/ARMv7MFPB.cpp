
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

errno_t ARMv7MFPB::init()
{
	errno_t ret = ap.read(REG_FP_CTRL, &ctrl.raw);
	if (ret != OK)
		return ret;

	bpList = std::vector<FP_COMP>();
	for (int i = 0; i < ctrl.num(); i++)
	{
		FP_COMP comp;
		ret = ap.read(REG_FP_COMP0 + (i * 4), &comp.raw);
		if (ret != OK)
			return ret;
		bpList.push_back(comp);
	}

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

int32_t ARMv7MFPB::findBreakPoint(uint32_t addr)
{
	uint32_t index = 0;
	for (auto e : bpList)
	{
		if (e.ENABLE && e.match(addr))
			return index;
		index++;
	}
	return -1;
}

int32_t ARMv7MFPB::findEmpty()
{
	uint32_t index = 0;
	for (auto e : bpList)
	{
		if (!bpList[index].ENABLE)
			return index;
		index++;
	}
	return -1;
}

errno_t ARMv7MFPB::addBreakPoint(uint32_t addr)
{
	if (!initialized)
		return EPERM;

	if (findBreakPoint(addr) >= 0)
		return OK;	// already exist

	int32_t index = findEmpty();
	if (index < 0)
		return EFAULT;

	return setBreakPoint(true, index, addr);
}

errno_t ARMv7MFPB::delBreakPoint(uint32_t addr)
{
	if (!initialized)
		return EPERM;

	int32_t index = findBreakPoint(addr);
	if (index < 0)
		return OK;	// not exist

	return setBreakPoint(false, index, addr);
}

errno_t ARMv7MFPB::setBreakPoint(bool enable, uint32_t index, uint32_t addr)
{
	errno_t ret;

	if (!initialized)
		return EPERM;

	// Code: 0x00000000 - 0x20000000
	if ((addr & 0x1) != 0 || (addr & 0xE0000000) != 0)
		return EINVAL;

	if (index >= ctrl.num())
		return EINVAL;

	// auto enable
	if (!isEnabled())
	{
		ret = setEnable();
		if (ret != OK)
			return ret;
	}

	FP_COMP comp;
	comp.raw = 0;
	comp.REPLACE = enable ? ((addr & 0x2) != 0 ? FP_COMP::UPPER_HALF : FP_COMP::LOWER_HALF) : FP_COMP::REMAP;
	comp.ENABLE = enable ? 1 : 0;
	comp.COMP = (addr & 0x1FFFFFFC) >> 2;
	ret = ap.write(REG_FP_COMP0 + (index * 4), comp.raw);
	if (ret == OK)
		bpList[index] = comp;
	return ret;
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
