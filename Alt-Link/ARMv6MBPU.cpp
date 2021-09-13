
#include "stdafx.h"
#include "ARMv6MBPU.h"

// v6-M
#define REG_BP_CTRL			(base + 0x000)
#define REG_BP_COMP0		(base + 0x008)
#define REG_BP_COMP1		(base + 0x008)
#define REG_BP_COMP2		(base + 0x008)
#define REG_BP_COMP3		(base + 0x008)

errno_t ARMv6MBPU::init()
{
	errno_t ret = ap.read(REG_BP_CTRL, &ctrl.raw);
	if (ret != OK)
		return ret;

	bpList = std::vector<BP_COMP>();
	for (uint32_t i = 0; i < ctrl.num(); i++)
	{
		BP_COMP comp;
		ret = ap.read(REG_BP_COMP0 + (i * 4), &comp.raw);
		if (ret != OK)
			return ret;
		bpList.push_back(comp);
	}

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

int32_t ARMv6MBPU::findBreakPoint(uint32_t addr)
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

int32_t ARMv6MBPU::findEmpty()
{
	uint32_t index = 0;
	for (auto e : bpList)
	{
		(void) e;
		if (!bpList[index].ENABLE)
			return index;
		index++;
	}
	return -1;
}

errno_t ARMv6MBPU::addBreakPoint(uint32_t addr)
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

errno_t ARMv6MBPU::delBreakPoint(uint32_t addr)
{
	if (!initialized)
		return EPERM;

	int32_t index = findBreakPoint(addr);
	if (index < 0)
		return OK;	// not exist

	return setBreakPoint(false, index, addr);
}

errno_t ARMv6MBPU::setBreakPoint(bool enable, uint32_t index, uint32_t addr)
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

	BP_COMP comp;
	comp.raw = 0;
	comp.BP_MATCH = enable ? ((addr & 0x2) != 0 ? BP_COMP::UPPER_HALF : BP_COMP::LOWER_HALF) : BP_COMP::NO_BP;
	comp.ENABLE = enable ? 1 : 0;
	comp.COMP = (addr & 0x1FFFFFFC) >> 2;
	ret = ap.write(REG_BP_COMP0 + (index * 4), comp.raw);
	if (ret == OK)
		bpList[index] = comp;
	return ret;
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
