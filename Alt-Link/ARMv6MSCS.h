
#pragma once

#include <cstdint>
#include "ADIv5.h"

class ARMv6MSCS : public ADIv5::Memory
{
public:
	union CPUID
	{
		struct
		{
			uint32_t Revision		: 4;
			uint32_t PartNo			: 12;
			uint32_t Architecture	: 4;
			uint32_t Variant		: 4;
			uint32_t Implementer	: 8;
		};
		uint32_t raw;
		void print();
	};
	static_assert(CONFIRM_UINT32(CPUID));

	union DFSR
	{
		struct
		{
			uint32_t HALTED			: 1;
			uint32_t BKPT			: 1;
			uint32_t DWTTRAP		: 1;
			uint32_t VCATCH			: 1;
			uint32_t EXTERNAL		: 1;
			uint32_t Reserved		: 27;
		};
		uint32_t raw;
		void print();
	};
	static_assert(CONFIRM_UINT32(DFSR));

	enum REGSEL : uint32_t
	{
		R0		= 0,
		R1		= 1,
		R2		= 2,
		R3		= 3,
		R4		= 4,
		R5		= 5,
		R6		= 6,
		R7		= 7,
		R8		= 8,
		R9		= 9,
		R10		= 10,
		R11		= 11,
		R12		= 12,
		SP		= 13,
		LR		= 14,
		DebugReturnAddress	= 15,
		xPSR	= 16,
		MSP		= 17,
		PSP		= 18,
		CONTROL_PRIMASK		= 20
	};

public:
	ARMv6MSCS(const Memory& memory) : Memory(memory) {}

	int32_t readCPUID(CPUID* cpuid);
	int32_t readDFSR(DFSR* dfsr);
	int32_t readReg(REGSEL reg, uint32_t* data);
	int32_t writeReg(REGSEL reg, uint32_t data);
	void printRegs();
	void printDHCSR();
	void printDEMCR();

	int32_t halt(bool maskIntr = false);
	int32_t run(bool maskIntr = false);
	int32_t step(bool maskIntr = false);

private:
	int32_t waitForRegReady();
};
