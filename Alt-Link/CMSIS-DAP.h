
#pragma once

#include <cstdint>
#include <vector>

#if defined(_WIN32)
#pragma comment(lib, "setupapi.lib")
#if defined(_DEBUG)
#pragma comment(lib, "hidapid.lib")
#else
#pragma comment(lib, "hidapi.lib")
#endif
#endif
#include <hidapi.h>

#include "TargetInterface.h"

/* FIXME error code */
#define CMSISDAP_OK								0
#define CMSISDAP_ERR_FATAL						1
#define CMSISDAP_ERR_INVALID_ARGUMENT			2
#define CMSISDAP_ERR_NO_MEMORY					3
#define CMSISDAP_ERR_DAP_RES					4
#define CMSISDAP_ERR_ACKFAULT					5
#define CMSISDAP_ERR_INVALID_TX_LEN				6
#define CMSISDAP_ERR_USBHID_INIT				7
#define CMSISDAP_ERR_USBHID_OPEN				8
#define CMSISDAP_ERR_USBHID_NOT_FOUND_DEVICE	9
#define CMSISDAP_ERR_USBHID_WRITE				10
#define CMSISDAP_ERR_USBHID_TIMEOUT				11
#define CMSISDAP_ERR_USBHID_EXIT				12
#define CMSISDAP_ERR_INVALID_STATUS				13

#define CONFIRM_SIZE(typeA, typeB) sizeof(typeA) == sizeof(typeB), "sizeof(" #typeA ") should be same as sizeof(" #typeB ")"
#define CONFIRM_UINT32(type) CONFIRM_SIZE(type, uint32_t)

class CMSISDAP : public TargetInterface
{
public:
	virtual int32_t attach();
	virtual void detach();

	virtual void setTargetThreadId();
	virtual void setCurrentPC(const uint64_t addr);

	virtual void resume();
	virtual int32_t step(uint8_t* signal);
	virtual int32_t interrupt(uint8_t* signal);

	virtual int32_t setBreakPoint(BreakPointType type, uint64_t addr, uint32_t kind);
	virtual int32_t unsetBreakPoint(BreakPointType type, uint64_t addr, uint32_t kind);

	virtual int32_t setWatchPoint(WatchPointType type, uint64_t addr, uint32_t kind);
	virtual int32_t unsetWatchPoint(WatchPointType type, uint64_t addr, uint32_t kind);

	virtual int32_t readRegister(const uint32_t n, uint32_t* out);
	virtual int32_t readRegister(const uint32_t n, uint64_t* out);
	virtual int32_t readRegister(const uint32_t n, uint64_t* out1, uint64_t* out2);	// 128-bit
	virtual int32_t writeRegister(const uint32_t n, const uint32_t data);
	virtual int32_t writeRegister(const uint32_t n, const uint64_t data);
	virtual int32_t writeRegister(const uint32_t n, const uint64_t data1, const uint64_t data2); // 128-bit
	virtual int32_t readGenericRegisters(std::vector<uint32_t>* array);
	virtual int32_t writeGenericRegisters(const std::vector<uint32_t>& array);

	virtual void readMemory(uint64_t addr, uint32_t len, std::vector<uint8_t>* array);
	virtual uint8_t writeMemory(uint64_t addr, uint32_t len, const std::vector<uint8_t>& array);

	virtual int32_t monitor(const std::string command, std::string* output);

	int32_t initialize(void);
	int32_t finalize(void) { dap.finalize(); }
	int32_t resetSw(void);
	int32_t resetHw(void);
	int32_t setSpeed(uint32_t speed);

private:

	class DAP
	{
	public:
		enum LED {
			CONNECT = 0,
			RUNNING = 1
		};
		int32_t initialize(void);
		int32_t finalize(void);

		int32_t cmdSwjPins(uint8_t isLevelHigh, uint8_t pin, uint32_t delay, uint8_t *input);
		int32_t cmdSwjClock(uint32_t clock);
		int32_t dpapRead(bool dp, uint32_t reg, uint32_t *data);
		int32_t dpapWrite(bool dp, uint32_t reg, uint32_t val);
		int32_t cmdLed(LED led, bool on);
		int32_t resetLink(void);

	private:
		enum CMD {
			CMD_INFO = 0x00,
			CMD_LED = 0x01,
			CMD_CONNECT = 0x02,
			CMD_DISCONNECT = 0x03,
			CMD_TX_CONF = 0x04,
			CMD_TX = 0x05,
			CMD_TX_BLOCK = 0x06,
			CMD_TX_ABORT = 0x07,
			CMD_WRITE_ABORT = 0x08,
			CMD_DELAY = 0x09,
			CMD_RESET_TARGET = 0x0A,
			CMD_SWJ_PINS = 0x10,
			CMD_SWJ_CLOCK = 0x11,
			CMD_SWJ_SEQ = 0x12,
			CMD_SWD_CONF = 0x13,
			CMD_JTAG_SEQ = 0x14,
			CMD_JTAG_CONFIGURE = 0x15,
			CMD_JTAG_IDCODE = 0x16,
		};

		enum INFO_ID {
			INFO_ID_VID = 0x00,         /* string */
			INFO_ID_PID = 0x02,         /* string */
			INFO_ID_SERNUM = 0x03,      /* string */
			INFO_ID_FW_VER = 0x04,      /* string */
			INFO_ID_TD_VEND = 0x05,     /* string */
			INFO_ID_TD_NAME = 0x06,     /* string */
			INFO_ID_CAPABILITIES = 0xf0,/* byte */
			INFO_ID_PKT_CNT = 0xfe,     /* byte */
			INFO_ID_PKT_SZ = 0xff,      /* short */
		};

		hid_device *hidHandle;
		uint8_t *packetBuf;
		char *fwver;
		char *name;
		char *vendor;
		uint32_t ap_bank_value;
		uint16_t packetBufSize;
		uint16_t packetMaxCount;
		uint16_t pid;
		uint16_t vid;
		uint8_t caps;

		int32_t usbOpen(void);
		int32_t usbClose(void);
		int32_t usbTx(uint32_t txlen);
		int32_t cmdInfoCapabilities(void);
		int32_t cmdConnect(void);
		int32_t cmdDisconnect(void);
		int32_t cmdTxConf(uint8_t idle, uint16_t delay, uint16_t retry);
		int32_t cmdInfoFwVer(void);
		int32_t cmdInfoVendor(void);
		int32_t cmdInfoName(void);
		int32_t cmdInfoPacketSize(void);
		int32_t cmdInfoPacketCount(void);
		int32_t getStatus(void);
		int32_t change2Swd(void);
		int32_t cmdSwdConf(uint8_t cfg);
	} dap;

	class DP
	{
	public:
		DP(DAP& _dap) : dap(_dap) {}
		int32_t read(uint32_t reg, uint32_t *data);
		int32_t write(uint32_t reg, uint32_t val);

	private:
		DAP& dap;
	} dp = DP(dap);

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
	} ap = AP(dap, dp);

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

	class Component
	{
	public:
		Component(MEM_AP& _ap, uint32_t _base) : ap(_ap), base(_base) {}

		int32_t read();
		void print();
		bool isRomTable();
		bool isARMv6MSCS();

	public:
		MEM_AP& ap;
		uint32_t base;

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

	class ARMv6MSCS
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
		ARMv6MSCS(MEM_AP& _ap, uint32_t _base) : ap(_ap), base(_base) {}

		int32_t readCPUID(CPUID* cpuid);
		int32_t readDFSR(DFSR* dfsr);
		int32_t readReg(REGSEL reg, uint32_t* data);
		int32_t writeReg(REGSEL reg, uint32_t data);
		void printCPUID(const CPUID& cpuid);
		void printDFSR(const DFSR& dfsr);
		void printRegs();
		void printDHCSR();
		void printDEMCR();

		int32_t halt(bool maskIntr = false);
		int32_t run(bool maskIntr = false);
		int32_t step(bool maskIntr = false);

	private:
		MEM_AP& ap;
		uint32_t base;

		CPUID cpuid;

		int32_t waitForRegReady();
	};
};
