
#pragma once

#include <cstdint>
#include "ADIv5.h"

union DBGDIDR;
union DBGDEVID;
union DBGDEVID1;

class ARMv7ARDIF : public ADIv5::Memory
{
public:
	ARMv7ARDIF(const Memory& memory) : Memory(memory) {}

	errno_t init(uint32_t _PART);
	errno_t getPC(uint32_t* pc);

private:
	union MPIDR
	{
		struct
		{
			uint32_t CPUID		: 2;
			uint32_t SBZ0		: 6;
			uint32_t CLUSTERID	: 4;
			uint32_t SBZ1		: 18;
			uint32_t U			: 1;
			uint32_t RAO		: 1;
		};
		uint32_t raw;
		void print();
	};
	static_assert(CONFIRM_UINT32(MPIDR));

	union DBGDIDR
	{
		enum DBGARCH
		{
			ARMv6_0		= 1,
			ARMv6_1		= 2,
			ARMv7_ALL	= 3,
			ARMv7_BASIC	= 4,
			ARMv7_1		= 5
		};

		struct
		{
			uint32_t Revision	: 4;	// bits[3:0] of Main ID Register
			uint32_t Variant	: 4;	// bits[23:20] of Main ID Register
			uint32_t UNK		: 4;
			uint32_t SE_imp		: 1;	// Security Extensions
			uint32_t PCSR_imp	: 1;	// PCSR is in register 33
			uint32_t nSUHD_imp	: 1;	// Secure User halting debug
			uint32_t DEVID_imp	: 1;	// DBGDEVID is imp. or not
			DBGARCH Version		: 4;
			uint32_t CTX_CMPs	: 4;	// context matching breakpoints 0b0000: 1, 0b1111: 16
			uint32_t BRPs		: 4;	// breakpoints 0b0001: 2, 0b1111: 16, 0b0000: reserved
			uint32_t WRPs		: 4;	// watchpoints 0b0000: 1, 0b1111: 16
		};
		uint32_t raw;
		void print();
		bool DEVID_Exists() { return (Version == ARMv7_1 || DEVID_imp == 1) ? true : false; }
		bool DEVID1_Exists();

	};
	static_assert(CONFIRM_UINT32(DBGDIDR));

	union DBGDEVID
	{
		enum PCSAMPLE
		{
			NOT_IMPLEMENTED		= 0,
			PCSR				= 1,	// register 40
			PCSR_CIDSR			= 2,	// register 40, 41
			PCSR_CIDSR_VIDSR	= 3		// register 40, 41, 42
		};

		struct
		{
			PCSAMPLE PCsample		: 4;
			uint32_t WPAddrMask		: 4;
			uint32_t BPAddrMask		: 4;
			uint32_t VectorCatch	: 4;
			uint32_t VirtExtns		: 4;
			uint32_t DoubleLock		: 4;
			uint32_t AuxRegs		: 4;
			uint32_t CIDMask		: 4;	// 0: Context ID masking is not implemented, 1: implemented
		};
		uint32_t raw;
		bool PCSR_Exists() { return (PCsample == PCSR || PCsample == PCSR_CIDSR || PCsample == PCSR_CIDSR_VIDSR) ? true : false; }
	};
	static_assert(CONFIRM_UINT32(DBGDEVID));

	union DBGDEVID1
	{
		enum TYPE
		{
			OFFSET		= 0,
			NO_OFFSET	= 1
		};

		struct
		{
			TYPE PCSROffset		: 4;
			uint32_t UNK		: 28;
		};
		uint32_t raw;
	};
	static_assert(CONFIRM_UINT32(DBGDEVID1));

	uint32_t PART;
	MPIDR mpidr;
	DBGDIDR didr;
	DBGDEVID devid;
	DBGDEVID1 devid1;
};