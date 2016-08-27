
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
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

private:
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
	ADIv5(std::shared_ptr<DAP> _dap) : dap(_dap), ap(*this, *_dap) {}

	int32_t getIDCODE(DP_IDCODE* idcode);
	int32_t getCtrlStat(DP_CTRL_STAT* ctrlStat);
	int32_t setCtrlStat(DP_CTRL_STAT& ctrlStat);
	errno_t clearError();
	int32_t powerupDebug();
	int32_t scanAPs();

	class AP
	{
	public:
		AP(ADIv5& _adi, DAP& _dap) : adi(_adi), dap(_dap) {}
		int32_t read(uint32_t ap, uint32_t reg, uint32_t *data);
		int32_t write(uint32_t ap, uint32_t reg, uint32_t val);

	private:
		ADIv5& adi;
		DAP& dap;
		uint32_t lastAp = 0;
		uint32_t lastApBank = 0;

		int32_t select(uint32_t ap, uint32_t reg);
		errno_t checkStatus(uint32_t ap);
	} ap;

	class MEM_AP
	{
	public:
		enum AccessSize
		{
			SIZE_8BIT	= 0,
			SIZE_16BIT	= 1,
			SIZE_32BIT	= 2,
			SIZE_64BIT	= 3,
			SIZE_128BIT	= 4,
			SIZE_256BIT	= 5,
			INVALID		= 0xFFFFFFFF
		};

		MEM_AP(uint32_t _index, AP& _ap) : index(_index), ap(_ap) {}
		errno_t read(uint32_t addr, uint32_t *data);
		errno_t write(uint32_t addr, uint32_t val);
		errno_t write(uint32_t addr, uint16_t val);
		errno_t write(uint32_t addr, uint8_t val);
		errno_t setAccessSize(AccessSize size);

	private:
		AP& ap;
		uint32_t index;
		uint32_t lastTAR = 0;
		AccessSize lastAccessSize = INVALID;

		bool isSameTAR(uint32_t addr);
		bool isSame32BitAlignedTAR(uint32_t addr, uint32_t* reg);
		bool is32BitAligned(uint32_t addr);
		bool is16BitAligned(uint32_t addr);
	};

	class Memory
	{
	public:
		Memory(MEM_AP& _ap, uint32_t _base) : ap(_ap), base(_base) {}

		MEM_AP& ap;
		uint32_t base;
	};

	class Component : public Memory
	{
	public:
		Component(const Memory& memory) : Memory(memory) {}

		enum
		{
			ARM_PART_SCS_M3			= 0,
			ARM_PART_ITM_M347		= 1,
			ARM_PART_DWT_M347		= 2,
			ARM_PART_FPB_M34		= 3,
			ARM_PART_CTI_M7			= 6,
			ARM_PART_SCS_M00P		= 8,
			ARM_PART_DWT_M00P		= 0xA,
			ARM_PART_BPU_M00P		= 0xB,
			ARM_PART_SCS_M47		= 0xC,
			ARM_PART_FPB_M7			= 0xE,
			ARM_PART_ECT			= 0x906,
			ARM_PART_ETB			= 0x907,
			ARM_PART_TF				= 0x908,
			ARM_PART_TPIU			= 0x912,
			ARM_PART_TPIU_M3		= 0x923,
			ARM_PART_ETM_M4			= 0x925,
			ARM_PART_PTM_A9			= 0x950,
			ARM_PART_PMU_A9			= 0x9A0,
			ARM_PART_TPIU_M4		= 0x9A1,
			ARM_PART_PMU_A5			= 0x9A5,
			ARM_PART_DEBUG_IF_A5	= 0xC05,
			ARM_PART_DEBUG_IF_A8	= 0xC08,
			ARM_PART_DEBUG_IF_A9	= 0xC09,
			ARM_PART_DEBUG_IF_R4	= 0xC14,
		};

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
			void print();
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
			void print();
			bool isARM() { return JEP106CONTINUATION == 0x4 && JEP106ID == 0x3B ? true : false; }
		};
		static_assert(CONFIRM_SIZE(PID, uint64_t));

		int32_t readPidCid();
		CID getCid() { return cid; }
		PID getPid() { return pid; }
		void print();
		void printName();

		bool isRomTable();
		bool isARMv7ARDIF();
		bool isARMv6MSCS();
		bool isARMv6MDWT();
		bool isARMv7MDWT();
		bool isARMv6MBPU();
		bool isARMv7MFPB();

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
		ROM_TABLE(std::shared_ptr<Component> _component) : component(_component) {}

		int32_t read();
		void each(std::function<void(std::shared_ptr<Component>)> func);
		bool isSysmem() { return sysmem; }

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
				uint32_t ADDRESS_OFFSET			: 20;
			};
			uint32_t raw;
			uint32_t addr() { return raw & 0xFFFFF000; }
			bool present() { return FORMAT && PRESENT && addr() != 0 ? true : false; }
		};
		static_assert(CONFIRM_SIZE(Entry, uint32_t));

	private:
		std::shared_ptr<Component> component;

		bool sysmem;
		std::vector<std::pair<Entry, ROM_TABLE>> children;
		std::vector<std::pair<Entry, std::shared_ptr<Component>>> entries;
	};

	std::vector<std::shared_ptr<Component>> find(std::function<bool(Component&)> func);
	std::vector<std::shared_ptr<Component>> findARMv7ARDIF();
	std::vector<std::shared_ptr<Component>> findARMv6MSCS();
	std::vector<std::shared_ptr<Component>> findARMv6MDWT();
	std::vector<std::shared_ptr<Component>> findARMv7MDWT();
	std::vector<std::shared_ptr<Component>> findARMv6MBPU();
	std::vector<std::shared_ptr<Component>> findARMv7MFPB();
	std::vector<std::shared_ptr<MEM_AP>> findSysmem();

private:
	std::vector<std::pair<std::shared_ptr<MEM_AP>, ROM_TABLE>> memAps;
	std::vector<std::shared_ptr<MEM_AP>> ahbSysmemAps;
	std::shared_ptr<DAP> dap;
};
