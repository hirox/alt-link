
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
#include <vector>

#include "DAP.h"

class CMSISDAP : public DAP
{
public:
	int32_t initialize(void);
	int32_t finalize(void);
	int32_t setSpeed(uint32_t speed);
	int32_t scanJtagDevices();
	void setDapIndex(uint8_t index) { dapIndex = index; }

public:
	virtual int32_t dpRead(uint32_t reg, uint32_t *data);
	virtual int32_t dpWrite(uint32_t reg, uint32_t val);
	virtual int32_t apRead(uint32_t reg, uint32_t *data);
	virtual int32_t apWrite(uint32_t reg, uint32_t val);
	virtual int32_t setConnectionType(ConnectionType type);

public:
	enum LED {
		CONNECT = 0,
		RUNNING = 1
	};

	int32_t cmdSwjPins(uint8_t value, uint8_t pin, uint32_t delay, uint8_t *input);
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

	union SequenceInfo
	{
		struct
		{
			uint32_t cycles		: 6;
			uint32_t TMS		: 1;
			uint32_t TDO		: 1;	// capture TDO data
			uint32_t padding	: 24;
		};
		uint8_t raw[4];
	};
	static_assert(CONFIRM_SIZE(SequenceInfo, uint32_t));

	union JTAG_IDCODE
	{
		struct
		{
			uint32_t RAO		: 1;
			uint32_t DESIGNER	: 11;
			uint32_t PARTNO		: 16;
			uint32_t VERSION	: 4;
		};
		uint32_t raw;
		bool isARM() { return (RAO == 1 && DESIGNER == 0x23B) ? true : false; };
		bool isOldARM() { return (RAO == 1 && DESIGNER == 0x787) ? true : false; };
	};
	static_assert(CONFIRM_UINT32(JTAG_IDCODE));

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

	uint8_t dapIndex;

	// JTAG
	std::vector<JTAG_IDCODE> jtagIDCODEs;
	std::vector<uint8_t> jtagIrLength;

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
	int32_t cmdConnect(uint8_t mode);
	int32_t cmdDisconnect();
	int32_t cmdTxConf(uint8_t idle, uint16_t delay, uint16_t retry);
	int32_t cmdInfoFwVer();
	int32_t cmdInfoVendor();
	int32_t cmdInfoName();
	int32_t cmdInfoPacketSize();
	int32_t cmdInfoPacketCount();
	int32_t getPinStatus();
	int32_t dpapRead(bool dp, uint32_t reg, uint32_t *data);
	int32_t dpapWrite(bool dp, uint32_t reg, uint32_t val);
	int32_t getInfo(uint32_t type, RxPacket* rx);

	// SWD
	int32_t cmdSwdConf(uint8_t cfg);

	// JTAG
	int32_t getJtagIDCODEs(std::vector<JTAG_IDCODE>* idcodes);
	int32_t findJtagDevices(uint32_t* num);
	int32_t cmdJtagConfigure(const std::vector<uint8_t>& irLength);
	int32_t cmdJtagSequence(SequenceInfo info, uint8_t* in, uint8_t* out);
	int32_t resetJtagTap();
	int32_t sendTms(uint8_t cycles, uint8_t tms, uint8_t tdi = 0, uint8_t* tdo = nullptr);

	// SWJ
	int32_t jtagToSwd();
	int32_t swdToJtag();
};
