
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

	union DEMCR
	{
		struct
		{
			uint32_t VC_CORERESET	: 1;
			uint32_t Reserved0		: 9;
			uint32_t VC_HARDERR		: 1;
			uint32_t Reserved1		: 13;
			uint32_t DWTENA			: 1;
			uint32_t Reserved2		: 7;
		};
		uint32_t raw;
		void print();
	};
	static_assert(CONFIRM_UINT32(DEMCR));

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
		CONTROL_PRIMASK		= 20,
		FPSCR	= 0x21,
		S0		= 0x40,
		S1		= 0x41,
		S2		= 0x42,
		S3		= 0x43,
		S4		= 0x44,
		S5		= 0x45,
		S6		= 0x46,
		S7		= 0x47,
		S8		= 0x48,
		S9		= 0x49,
		S10		= 0x4A,
		S11		= 0x4B,
		S12		= 0x4C,
		S13		= 0x4D,
		S14		= 0x4E,
		S15		= 0x4F,
		S16		= 0x50,
		S17		= 0x51,
		S18		= 0x52,
		S19		= 0x53,
		S20		= 0x54,
		S21		= 0x55,
		S22		= 0x56,
		S23		= 0x57,
		S24		= 0x58,
		S25		= 0x59,
		S26		= 0x5A,
		S27		= 0x5B,
		S28		= 0x5C,
		S29		= 0x5D,
		S30		= 0x5E,
		S31		= 0x5F
	};

public:
	ARMv6MSCS(const Memory& memory) : Memory(memory) {}

	errno_t readCPUID(CPUID* cpuid);
	errno_t readDFSR(DFSR* dfsr);
	errno_t readDEMCR(DEMCR* demcr);
	errno_t writeDEMCR(DEMCR& demcr);
	errno_t readReg(REGSEL reg, uint32_t* data);
	errno_t writeReg(REGSEL reg, uint32_t data);
	void printRegs();
	void printDHCSR();

	errno_t isHalt(bool* halt);

	int32_t halt(bool maskIntr = false);
	int32_t run(bool maskIntr = false);
	int32_t step(bool maskIntr = false);

private:
	int32_t waitForRegReady();
};
