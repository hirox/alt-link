
#pragma once

#include <cstdint>
#include <vector>
#include "DAP.h"

class ADIv5
{
public:
	union DP_IDCODE
	{
		struct
		{
			uint32_t RAO		: 1;
			uint32_t DESIGNER	: 11;
			uint32_t VERSION	: 4;
			uint32_t MIN		: 1;
			uint32_t RES0		: 3;
			uint32_t PARTNO		: 8;
			uint32_t REVISION	: 4;
		};
		uint32_t raw;
		void print();
	};
	static_assert(CONFIRM_UINT32(DP_IDCODE));

	union DP_CTRL_STAT
	{
		struct
		{
			uint32_t ORUNDETECT		: 1;
			uint32_t STICKYORUN		: 1;
			uint32_t TRNMODE		: 2;
			uint32_t STICKYCMP		: 1;
			uint32_t STICKYERR		: 1;
			uint32_t READOK			: 1;
			uint32_t WDATAERR		: 1;
			uint32_t MASKLANE		: 4;
			uint32_t TRNCNT			: 12;
			uint32_t RES0			: 2;
			uint32_t CDBGRSTREQ		: 1;	// DBG RST REQ
			uint32_t CDBGRSTACK		: 1;	// DBG RST ACK
			uint32_t CDBGPWRUPREQ	: 1;	// DBG PWR UP REQ
			uint32_t CDBGPWRUPACK	: 1;	// DBG PWR UP ACK
			uint32_t CSYSPWRUPREQ	: 1;	// SYS PWR UP REQ
			uint32_t CSYSPWRUPACK	: 1;	// SYS PWR UP ACK
		};
		uint32_t raw;
		void print();
	};
	static_assert(CONFIRM_UINT32(DP_CTRL_STAT));

public:
	ADIv5(DAP& dap) : dp(dap), ap(dap, dp) {}

	int32_t getIDCODE(DP_IDCODE* idcode);
	int32_t getCtrlStat(DP_CTRL_STAT* ctrlStat);
	int32_t setCtrlStat(DP_CTRL_STAT& ctrlStat);
	int32_t powerupDebug();
	int32_t scanAPs();

	class DP
	{
	public:
		DP(DAP& _dap) : dap(_dap) {}
		int32_t read(uint32_t reg, uint32_t *data);
		int32_t write(uint32_t reg, uint32_t val);

	private:
		DAP& dap;
	} dp;

	class AP
	{
	public:
		AP(DAP& _dap, DP& _dp) : dap(_dap), dp(_dp) {}
		int32_t read(uint32_t ap, uint32_t reg, uint32_t *data);
		int32_t write(uint32_t ap, uint32_t reg, uint32_t val);

	private:
		DAP& dap;
		DP& dp;
		uint32_t lastAp = 0;
		uint32_t lastApBank = 0;

		int32_t select(uint32_t ap, uint32_t reg);
	} ap;

	class MEM_AP
	{
	public:
		MEM_AP(uint32_t _index, AP& _ap) : index(_index), ap(_ap) {}
		int32_t read(uint32_t addr, uint32_t *data);
		int32_t write(uint32_t addr, uint32_t val);

	private:
		AP& ap;
		uint32_t index;
	};

	class Memory
	{
	public:
		Memory(MEM_AP& _ap, uint32_t _base) : ap(_ap), base(_base) {}
		Memory(Memory& memory) : ap(memory.ap), base(memory.base) {}

		MEM_AP& ap;
		uint32_t base;
	};

	class Component : public Memory
	{
	public:
		Component(Memory& memory) : Memory(memory) {}

		int32_t read();
		void print();
		bool isRomTable();
		bool isARMv6MSCS();
		bool isARMv6MDWT();
		bool isARMv7MDWT();

	private:
		union CID
		{
			enum Class : uint32_t
			{
				GENERIC_VERIFICATION	= 0x0,
				ROM_TABLE				= 0x1,
				DEBUG_COMPONENT			= 0x9,
				PERIPHERAL_TEST_BLOCK	= 0xB,
				OPTIMO_DE				= 0xD,
				GENERIC_IP				= 0xE,
				PRIME_CELL				= 0xF
			};

			struct
			{
				uint32_t Preamble0		: 12;
				Class ComponentClass	: 4;
				uint32_t Preamble1		: 16;
			};
			struct
			{
				uint8_t uint8[4];
			};
			uint32_t raw;
		};
		static_assert(CONFIRM_SIZE(CID, uint32_t));

		union PID
		{
			struct
			{
				uint32_t PART				: 12;
				uint32_t JEP106ID			: 7;
				uint32_t JEDEC				: 1;
				uint32_t REVISION			: 4;
				uint32_t CMOD				: 4;
				uint32_t REVAND				: 4;
				uint32_t JEP106CONTINUATION	: 4;
				uint32_t SIZE				: 4;
				uint32_t RES0				: 24;
			};
			struct
			{
				uint8_t uint8[8];
			};
			uint64_t raw;

			bool isARM() { return JEP106CONTINUATION == 0x4 && JEP106ID == 0x3B ? true : false; }
		};
		static_assert(CONFIRM_SIZE(PID, uint64_t));

	private:
		CID cid;
		PID pid;

	private:
		int32_t readPid();
		int32_t readCid();
		const char* getName();
	};

	class ROM_TABLE
	{
	public:
		ROM_TABLE(Component& _component) : component(_component) {}

		int32_t read();

	private:
		union Entry
		{
			struct
			{
				uint32_t PRESENT				: 1;
				uint32_t FORMAT					: 1;
				uint32_t PWR_DOMAIN_ID_VAILD	: 1;
				uint32_t RESERVED0				: 1;
				uint32_t PWR_DOMAIN_ID			: 5;
				uint32_t RESERVED1				: 3;
				uint32_t ADDRESS_OFFSET			: 24;
			};
			uint32_t raw;
			uint32_t addr() { return raw & 0xFFFFF000; }
			bool present() { return FORMAT && PRESENT && addr() != 0 ? true : false; }
		};

	private:
		Component& component;

		bool sysmem;
		std::vector<std::pair<Entry, Component>> entries;
	};

	class ARMv6MSCS : public Memory
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
		ARMv6MSCS(Memory& memory) : Memory(memory) {}

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
		CPUID cpuid;

		int32_t waitForRegReady();
	};

	class ARMv6MDWT : public Memory
	{
	public:
		ARMv6MDWT(Memory& memory) : Memory(memory) {}

		int32_t getPC(uint32_t* pc);
		void printPC();
		void printCtrl();
	};

	class ARMv7MDWT : public ARMv6MDWT
	{
	public:
		ARMv7MDWT(Memory& memory) : ARMv6MDWT(memory) {}

		void printCtrl();
	};
};
