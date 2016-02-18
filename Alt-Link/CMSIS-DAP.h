
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
#include <string>

#include "DAP.h"

class CMSISDAP : public DAP
{
public:
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
	uint16_t packetMaxSize;
	uint16_t packetMaxCount;
	uint16_t pid;
	uint16_t vid;
	Caps caps;

	class TxPacket
	{
	private:
		uint8_t _data[64 + 1];
		uint32_t written;

	public:
		TxPacket() : written(0) {}

		int32_t write(uint8_t data);
		int32_t write16(uint16_t data);
		int32_t write32(uint32_t data);
		void clear() { written = 0; }
		const uint8_t* data() const { return _data; }
		uint32_t length() const { return written; }
	};

	class RxPacket
	{
	private:
		uint8_t _data[64];
		uint32_t _length;

	public:
		RxPacket() : _length(sizeof(_data)) {}

		uint8_t* data() { return _data; }
		uint32_t length() const { return _length; }
		void length(uint32_t len) { _length = len; }
	};

	int32_t usbOpen();
	int32_t usbClose();
	int32_t usbTx(uint32_t txlen);
	int32_t usbTx(const TxPacket& packet);
	int32_t usbRx(RxPacket* rx);
	int32_t usbTxRx(const TxPacket& tx, RxPacket* rx);
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

	int32_t getInfo(uint32_t type, RxPacket* rx);
};
