
#pragma once

#include "stdafx.h"

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
#include "DAP.h"

class CMSISDAP : public TargetInterface, public DAP
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
	int32_t finalize(void);
	int32_t resetSw(void);
	int32_t resetHw(void);
	int32_t setSpeed(uint32_t speed);
	int32_t jtagToSwd();

public:
	virtual int32_t dpRead(uint32_t reg, uint32_t *data);
	virtual int32_t dpWrite(uint32_t reg, uint32_t val);
	virtual int32_t apRead(uint32_t reg, uint32_t *data);
	virtual int32_t apWrite(uint32_t reg, uint32_t val);

public:
	enum LED {
		CONNECT = 0,
		RUNNING = 1
	};

	int32_t cmdSwjPins(uint8_t isLevelHigh, uint8_t pin, uint32_t delay, uint8_t *input);
	int32_t cmdSwjClock(uint32_t clock);
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

	union Caps
	{
		struct
		{
			uint32_t SWD		: 1;
			uint32_t JTAG		: 1;
			uint32_t Reserved	: 30;
		};
		uint32_t raw;
	};
	static_assert(CONFIRM_UINT32(Caps));

	hid_device *hidHandle;
	uint8_t *packetBuf;
	std::string fwver;
	std::string name;
	std::string vendor;
	uint32_t ap_bank_value;
	uint16_t packetBufSize;
	uint16_t packetMaxCount;
	uint16_t pid;
	uint16_t vid;
	Caps caps;

	int32_t usbOpen();
	int32_t usbClose();
	int32_t usbTx(uint32_t txlen);
	int32_t cmdInfoCapabilities();
	int32_t cmdConnect();
	int32_t cmdDisconnect();
	int32_t cmdTxConf(uint8_t idle, uint16_t delay, uint16_t retry);
	int32_t cmdInfoFwVer();
	int32_t cmdInfoVendor();
	int32_t cmdInfoName();
	int32_t cmdInfoPacketSize();
	int32_t cmdInfoPacketCount();
	int32_t getStatus();
	int32_t cmdSwdConf(uint8_t cfg);
	int32_t dpapRead(bool dp, uint32_t reg, uint32_t *data);
	int32_t dpapWrite(bool dp, uint32_t reg, uint32_t val);
};
