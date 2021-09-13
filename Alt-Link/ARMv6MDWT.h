
#pragma once

#include <cstdint>
#include "ADIv5.h"

class ARMv6MDWT : public ADIv5::Memory
{
public:
	ARMv6MDWT(const Memory& memory) : Memory(memory) {}
	virtual ~ARMv6MDWT() {}

	int32_t getPC(uint32_t* pc);
	void printPC();
	virtual void printCtrl();
};

class ARMv7MDWT : public ARMv6MDWT
{
public:
	ARMv7MDWT(Memory& memory) : ARMv6MDWT(memory) {}

	virtual void printCtrl();
};