
#include "stdafx.h"

#include "CMSIS-DAP.h"
#include "ADIv5.h"
#include "JEP106.h"

#define USE_USB_TX_DBG 0

/*
 * based on CMSIS-DAP Beta 0.01.
 * https://silver.arm.com/browse/CMSISDAP
 * Version 1.1.0
 * http://www.keil.com/pack/doc/cmsis/DAP/html/index.html
 *
 * CoreSight Components Technical Reference Manual
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0432cj/Bcgbfdhc.html
 */

#define _USB_HID_REPORT_NUM 0x00

#define _CMSISDAP_DEFAULT_PACKET_SIZE (64 + 1) /* 64 bytes + 1 byte(hid report id) */
#define _CMSISDAP_MAX_CLOCK (10 * 1000 * 1000) /* Hz */
#define _CMSISDAP_USB_TIMEOUT 1000             /* ms */

static inline uint32_t buf2LE32(const uint8_t *buf)
{
	return (uint32_t)(buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24);
}

#define DAP_MODE_DEFAULT	0x0
#define DAP_MODE_SWD		0x1
#define DAP_MODE_JTAG		0x2

/* DAP Status Code */
#define _DAP_RES_OK 0
#define _DAP_RES_ERR 0xFF

#define _PIN_SWCLK (1 << 0)  /* SWCLK/TCK */
#define _PIN_SWDIO (1 << 1)  /* SWDIO/TMS */
#define _PIN_TDI (1 << 2)    /* TDI */
#define _PIN_TDO (1 << 3)    /* TDO */
#define _PIN_nTRST (1 << 5)  /* nTRST */
#define _PIN_nRESET (1 << 7) /* nRESET */

#define _TX_RES_OK 0x1
#define _TX_RES_WAIT 0x2
#define _TX_RES_SWD_ERROR 0x4
#define _TX_RES_VALUE_MISMATCH 0x8

#define AP_ABORT_DAPABORT 0x01     /* generate a DAP abort */
#define AP_ABORT_STK_CMP_CLR 0x02  /* clear STICKYCMP sticky compare flag */
#define AP_ABORT_STK_ERR_CLR 0x04  /* clear STICKYERR sticky error flag */
#define AP_ABORT_WD_ERR_CLR 0x08   /* clear WDATAERR write data error flag */
#define AP_ABORT_ORUN_ERR_CLR 0x10 /* clear STICKYORUN overrun error flag */

union TransferRequest
{
	struct
	{
		uint32_t APnDP		: 1;	// 0 = DP, 1 = AP
		uint32_t RnW		: 1;	// 0 = Write, 1 = Read
		uint32_t A32		: 2;
		uint32_t ValueMatch	: 1;
		uint32_t MatchMask	: 1;
		uint32_t Reserved	: 2;
		uint32_t Padding	: 24;
	};
	uint8_t raw[4];

	void setAP() { APnDP = 1; }
	void setDP() { APnDP = 0; }
	void setRead() { RnW = 1; }
	void setWrite() { RnW = 0; }
	void setRegister(uint32_t reg) { A32 = (reg & 0xC) >> 2; }
};
static_assert(CONFIRM_SIZE(TransferRequest, uint32_t));

/* three-bit ACK values for SWD access (sent LSB first) */
#define TX_ACK_OK 0x1
#define TX_ACK_WAIT 0x2
#define TX_ACK_FAULT 0x4
#define TX_ACK_NO_ACK 0x7
#define TX_ACK_MASK 0x7

#define WCR_TO_TRN(wcr) ((uint32_t)(1 + (3 & ((wcr)) >> 8))) /* 1..4 clocks */
#define WCR_TO_PRESCALE(wcr) ((uint32_t)(7 & ((wcr))))       /* impl defined */

int32_t CMSISDAP::enumerate(std::vector<DeviceInfo>* devices)
{
	if (devices == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	devices->clear();

	struct hid_device_info *info;
	struct hid_device_info *infoCur;

	infoCur = info = hid_enumerate(0x0, 0x0);
	while (infoCur != nullptr) {
#if 0
		printf("type: %04hx %04hx\npath: %s\nserial_number: %ls\n", infoCur->vendor_id, infoCur->product_id, infoCur->path, infoCur->serial_number);
		printf("Manu    : %ls\n", infoCur->manufacturer_string);
		printf("Product : %ls\n", infoCur->product_string);
#endif
		if (infoCur->product_string != NULL &&
			wcsstr(infoCur->product_string, L"CMSIS-DAP"))
		{
			DeviceInfo deviceInfo;
			deviceInfo.path = infoCur->path;
			deviceInfo.productString = infoCur->product_string;
			deviceInfo.serial = infoCur->serial_number;
			deviceInfo.vid = infoCur->vendor_id;
			deviceInfo.pid = infoCur->product_id;
			devices->push_back(deviceInfo);
		}
		infoCur = infoCur->next;
	}

	if (info != NULL)
	{
		hid_free_enumeration(info);
	}

}

std::shared_ptr<CMSISDAP> CMSISDAP::open(DeviceInfo& info)
{
	if (hid_init() != 0)
	{
		return std::shared_ptr<CMSISDAP>();
	}

	hid_device* handle = hid_open(info.vid, info.pid, NULL);
	if (handle == NULL)
	{
		_DBGPRT("hid_open failed.\n");
		return std::shared_ptr<CMSISDAP>();
	}

	return std::shared_ptr<CMSISDAP>(new CMSISDAP(handle, info.vid, info.pid, _CMSISDAP_DEFAULT_PACKET_SIZE)); 
}

CMSISDAP::~CMSISDAP()
{
	int ret = cmdDisconnect();
	if (ret != OK)
	{
		_ERRPRT("Failed to disconnect. (0x%08x)\n", ret);
		return;
	}

	ret = cmdLed(RUNNING, 0);
	if (ret != OK)
	{
		_ERRPRT("Failed to set LED. (0x%08x)\n", ret);
		return;
	}

	ret = cmdLed(CONNECT, 0);
	if (ret != OK)
	{
		_ERRPRT("Failed to set LED. (0x%08x)\n", ret);
		return;
	}

	hid_close(hidHandle);
	if (hid_exit() != 0)
	{
		return;
	}
}

int32_t CMSISDAP::usbTx(const TxPacket& packet)
{
	int ret = hid_write(hidHandle, packet.data(), packet.length());
	if (ret == -1)
		return CMSISDAP_ERR_USBHID_WRITE;

	return OK;
}

int32_t CMSISDAP::usbRx(RxPacket* rx)
{
	if (rx == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	int ret = hid_read_timeout(hidHandle, rx->data(), rx->length(), _CMSISDAP_USB_TIMEOUT);
	if (ret == -1 || ret == 0)
		return CMSISDAP_ERR_USBHID_TIMEOUT;

	rx->length(ret);
	return OK;
}

int32_t CMSISDAP::usbTxRx(const TxPacket& tx, RxPacket* rx)
{
	int ret;
	ret = usbTx(tx);
	if (ret != OK)
		return ret;

	return usbRx(rx);
}

int32_t CMSISDAP::TxPacket::write(uint8_t value)
{
	if (sizeof(_data) <= written)
		return CMSISDAP_ERR_NO_MEMORY;

	_data[written++] = value;

	return OK;
}

int32_t CMSISDAP::TxPacket::write16(uint16_t value)
{
	int32_t ret;
	ret = write(value & 0xFF);
	if (ret != OK)
		return ret;

	return write((value >> 8) & 0xFF);
}

int32_t CMSISDAP::TxPacket::write32(uint32_t value)
{
	int32_t ret;
	ret = write(value & 0xFF);
	if (ret != OK)
		return ret;

	ret = write((value >> 8) & 0xFF);
	if (ret != OK)
		return ret;

	ret = write((value >> 16) & 0xFF);
	if (ret != OK)
		return ret;

	return write((value >> 24) & 0xFF);
}

int32_t CMSISDAP::cmdLed(LED led, bool on)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_LED);
	tx.write(led);
	tx.write(on ? 1 : 0);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}
	uint8_t* data = rx.data();
	if (data[1] != _DAP_RES_OK) {
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdConnect(uint8_t mode)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_CONNECT);
	tx.write(mode);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	uint8_t* data = rx.data();
	if (data[1] != mode) {
		return CMSISDAP_ERR_FATAL;
	}

	return OK;
}

int32_t CMSISDAP::cmdDisconnect(void)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_DISCONNECT);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	uint8_t* data = rx.data();
	if (data[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdTxConf(uint8_t idle, uint16_t delay, uint16_t retry)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_TX_CONF);
	tx.write(idle);
	tx.write16(delay);
	tx.write16(retry);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	uint8_t* data = rx.data();
	if (data[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::getInfo(uint32_t type, RxPacket* rx)
{
	int ret;
	TxPacket packet;

	if (rx == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	packet.write(_USB_HID_REPORT_NUM);
	packet.write(CMD_INFO);
	packet.write(type);

	ret = usbTxRx(packet, rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	return OK;
}

int32_t CMSISDAP::cmdInfoCapabilities(void)
{
	RxPacket packet;
	int ret = getInfo(INFO_ID_CAPABILITIES, &packet);
	if (ret != OK)
		return ret;

	uint8_t* data = packet.data();
	if (data[1] != 1)
		return CMSISDAP_ERR_DAP_RES;

	caps.raw = data[2];
	return OK;
}

int32_t CMSISDAP::cmdInfoFwVer(void)
{
	RxPacket packet;
	int ret = getInfo(INFO_ID_FW_VER, &packet);
	if (ret != OK)
		return ret;

	uint8_t* data = packet.data();
	if (data[1] > 0)
		fwver = std::string((const char *)&(data[2]));
	else
		fwver = "";

	return OK;
}

int32_t CMSISDAP::cmdInfoVendor(void)
{
	RxPacket packet;
	int ret = getInfo(INFO_ID_TD_VEND, &packet);
	if (ret != OK)
		return ret;

	uint8_t* data = packet.data();
	if (data[1] > 0)
		vendor = std::string((const char *)&(data[2]));
	else
		vendor = "";

	return OK;
}

int32_t CMSISDAP::cmdInfoName(void)
{
	RxPacket packet;
	int ret = getInfo(INFO_ID_TD_NAME, &packet);
	if (ret != OK)
		return ret;

	uint8_t* data = packet.data();
	if (data[1] > 0)
		name = std::string((const char *)&(data[2]));
	else
		name = "";

	return OK;
}

int32_t CMSISDAP::cmdInfoPacketSize(void)
{
	RxPacket packet;
	int ret = getInfo(INFO_ID_PKT_SZ, &packet);
	if (ret != OK)
		return ret;

	uint8_t* data = packet.data();
	if (data[1] != 2) {
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	uint16_t size = data[2] + (data[3] << 8);

	// [TODO] packetMaxSize に対応
	if (packetMaxSize != size + 1)
		packetMaxSize = size + 1;

	return OK;
}

int32_t CMSISDAP::cmdInfoPacketCount(void)
{
	RxPacket packet;
	int ret = getInfo(INFO_ID_PKT_CNT, &packet);
	if (ret != OK)
		return ret;

	uint8_t* data = packet.data();
	if (data[1] != 1) { /* resは1byte */
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	// [TODO] packetMaxCount に対応
	packetMaxCount = data[2];
	return OK;
}

int32_t CMSISDAP::getPinStatus(void)
{
	int ret;
	uint8_t d;

	ret = cmdSwjPins(0, 0, 0, &d);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	_DBGPRT("SWCLK/TCK:%d SWDIO/TMS:%d TDI:%d TDO:%d !TRST:%d !RESET:%d\n",
		(d & _PIN_SWCLK) ? 1 : 0, (d & _PIN_SWDIO) ? 1 : 0,
		(d & _PIN_TDI) ? 1 : 0, (d & _PIN_TDO) ? 1 : 0,
		(d & _PIN_nTRST) ? 1 : 0, (d & _PIN_nRESET) ? 1 : 0);

	return OK;
}

int32_t CMSISDAP::jtagToSwd(void)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(7 * 8);
	tx.write32(0xFFFFFFFF);
	tx.write16(0xFFFF);
	tx.write(0xFF);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;


	tx.clear();
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(2 * 8);
	tx.write(0x9E);
	tx.write(0xE7);

	ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;


	tx.clear();
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(7 * 8);
	tx.write32(0xFFFFFFFF);
	tx.write16(0xFFFF);
	tx.write(0xFF);

	ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;


	/* 8 cycle idle period */
	tx.clear();
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(8);
	tx.write(0);

	ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;

	return OK;
}

int32_t CMSISDAP::swdToJtag(void)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(7 * 8);
	tx.write32(0xFFFFFFFF);
	tx.write16(0xFFFF);
	tx.write(0xFF);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;


	tx.clear();
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(2 * 8);
	tx.write(0x3C);
	tx.write(0xE7);

	ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;


	tx.clear();
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(1 * 8);
	tx.write(0xFF);

	ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;


	/* 8 cycle idle period */
	tx.clear();
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_SEQ);
	tx.write(8);
	tx.write(0);

	ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
		return CMSISDAP_ERR_DAP_RES;

	return OK;
}

int32_t CMSISDAP::resetLink(void)
{
#if 0
	/* アダプタにターゲットに応じたリセット方法が書かれている場合に発動させる */
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_RESET_TARGET;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	_DBGPRT("Target Reset Res: Status:%02x Execute:%s\n", packetBuf[1], packetBuf[2] == 0x1 ? "OK" : "no impl");
#endif

	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_WRITE_ABORT);
	tx.write(0x00); /* DAP Index, ignored in the swd. */
	tx.write(AP_ABORT_STK_CMP_CLR | AP_ABORT_WD_ERR_CLR | AP_ABORT_ORUN_ERR_CLR | AP_ABORT_STK_ERR_CLR);
	tx.write(0x00); /* SBZ */
	tx.write(0x00); /* SBZ */
	tx.write(0x00); /* SBZ */

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	if (rx.data()[1] != _DAP_RES_OK) {
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdSwjPins(uint8_t value, uint8_t pin, uint32_t delay, uint8_t *input)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_PINS);
	tx.write(value);
	tx.write(pin);
	tx.write32(delay);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (input != NULL)
	{
		*input = rx.data()[1];
	}

	return ret;
}

int32_t CMSISDAP::cmdSwjClock(uint32_t clock)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWJ_CLOCK);
	tx.write32(clock);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdSwdConf(uint8_t cfg)
{
	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_SWD_CONF);
	tx.write32(cfg);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (rx.data()[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::resetJtagTap()
{
	// go to test logic reset state
	int32_t ret = sendTms(5, 1);
	if (ret != OK)
		return ret;

	// toggle nTRST
	ret = cmdSwjPins(0, _PIN_nTRST, 0, nullptr);
	if (ret != OK)
		return ret;
	
	uint8_t value;
	ret = cmdSwjPins(_PIN_nTRST, _PIN_nTRST, 0, &value);
	if (ret != OK)
		return ret;

	if ((value & _PIN_nTRST) == 0)
	{
		static bool isFirst = true;
		if (isFirst)
		{
			_DBGPRT("[Warning] Failed to control nTRST pin. Please pull-up nTRST pin manually.\n");
			isFirst = false;
		}
	}

	// go to run-test idle
	ret = sendTms(1, 0);
	if (ret != OK)
		return ret;

	return OK;
}

int32_t CMSISDAP::findJtagDevices(uint32_t* num)
{
	if (num == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	int32_t ret = resetJtagTap();
	if (ret != OK)
		return ret;

	// go to Shift-IR
	ret = sendTms(1, 0);
	if (ret != OK)
		return ret;
	ret = sendTms(2, 1);
	if (ret != OK)
		return ret;
	ret = sendTms(2, 0);
	if (ret != OK)
		return ret;

	// make all devices to BYPASS mode (1280bits)
	for (uint32_t i = 0; i < 20; i++)
	{
		ret = sendTms(64, 0, 0xFF);
		if (ret != OK)
			return ret;
	}
	// send last bit and exit Shift-IR
	ret = sendTms(1, 1, 0xFF);
	if (ret != OK)
		return ret;

	// go to Shift-DR from Exit1-IR
	ret = sendTms(2, 1);
	if (ret != OK)
		return ret;
	ret = sendTms(2, 0);
	if (ret != OK)
		return ret;

	// flush all DR (320bits)
	for (uint32_t i = 0; i < 5; i++)
	{
		ret = sendTms(64, 0, 0);
		if (ret != OK)
			return ret;
	}

	uint32_t count = 0;
	while (1)
	{
		uint8_t out = 0;
		ret = sendTms(1, 0, 0xFF, &out);
		if (out != 0)
		{
			*num = count;
			_DBGPRT("JTAG Devices: %d\n", count);
			return OK;
		}

		count++;
		if (count > 320)
			return ENODEV;
	}

	return OK;
}

int32_t CMSISDAP::getJtagIDCODEs(std::vector<JTAG_IDCODE>* idcodes)
{
	if (idcodes == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	uint32_t num;
	int32_t ret = findJtagDevices(&num);
	if (ret != OK)
	{
		_ERRPRT("Failed to find jtag devices. (0x%08x)\n", ret);
		return ret;
	}

	ret = resetJtagTap();
	if (ret != OK)
		return ret;

	// go to Shift-DR
	ret = sendTms(1, 1);
	if (ret != OK)
		return ret;
	ret = sendTms(2, 0);
	if (ret != OK)
		return ret;

	for (uint32_t i = 0; i < num; i++)
	{
		JTAG_IDCODE idcode = { 0 };
		for (uint32_t j = 0; j < 4; j++)
		{
			uint8_t value;
			ret = sendTms(8, 0, 0, &value);
			if (ret != OK)
				return ret;
			idcode.raw |= (value << (8 * j));
		}
		idcodes->push_back(idcode);
	}
	return OK;
}

int32_t CMSISDAP::scanJtagDevices()
{
	int32_t ret = getJtagIDCODEs(&jtagIDCODEs);
	if (ret != OK)
	{
		_ERRPRT("Failed to get IDCODEs. (0x%08x)\n", ret);
		return ret;
	}

	if (jtagIDCODEs.size() == 0)
	{
		_ERRPRT("No JTAG devices found.\n");
		return OK;
	}

	for (uint32_t i = 0; i < jtagIDCODEs.size(); i++)
	{
		auto idcode = jtagIDCODEs[i];

		_DBGPRT("  Index: %d\n", i);
		if (idcode.RAO)
			_DBGPRT("    Designer: %s (%x)\n", getJEP106DesignerName(idcode.DESIGNER >> 7, idcode.DESIGNER & 0x7F), idcode.DESIGNER);
		else
			_DBGPRT("    Designer: UNKNOWN (%x,%x)\n", idcode.DESIGNER, idcode.raw);

		if (idcode.isARM())
		{
			if (idcode.PARTNO == 0xBA00)
			{
				_DBGPRT("    Device  : JTAG-DP (VER.%d) (0x%08x)\n", idcode.VERSION, idcode.raw);
				setDapIndex(i);
				jtagIrLength.push_back(4);
			}
			else if (idcode.PARTNO == 0xBA01)
			{
				// SW-DP のようだけど JTAG-DP みたい
				_DBGPRT("    Device  : JTAG-DP (VER.%d) (0x%08x)\n", idcode.VERSION, idcode.raw);
				jtagIrLength.push_back(4);
			}
#if 0
			else if (idcode.PARTNO == 0xBA02)
			{
				_DBGPRT("    Device  : SW-DP Multidrop support (VER.%d) (0x%08x)\n", idcode.VERSION, idcode.raw);
				jtagIrLength.push_back(4);
			}
#endif
			else
			{
				_DBGPRT("    Device  : Unknown (PARTNO %x, VER.%d) (0x%08x)\n", idcode.PARTNO, idcode.VERSION, idcode.raw);
				jtagIrLength.push_back(4);
			}
		}
		else if (idcode.isOldARM())
		{
			switch (idcode.PARTNO)
			{
			case 0x7B37:
				_DBGPRT("    Device  : ARM ARM11 MPCore (REV.%d) (0x%08x)\n", idcode.VERSION, idcode.raw);
				jtagIrLength.push_back(5);
			case 0x5966:
				_DBGPRT("    Device  : ARM ARM966E-S (REV.%d) (0x%08x)\n", idcode.VERSION, idcode.raw);
				jtagIrLength.push_back(4);
				break;
			}
		}
		else
		{
			_DBGPRT("    Device  : Unknown (PARTNO %x, VER.%d) (0x%08x)\n", idcode.PARTNO, idcode.VERSION, idcode.raw);
			jtagIrLength.push_back(4);
		}
	}

	if (jtagIrLength.size() > 0)
	{
		cmdJtagConfigure(jtagIrLength);
	}

	ret = resetJtagTap();
	if (ret != OK)
		return ret;

	return OK;
}

int32_t CMSISDAP::sendTms(uint8_t cycles, uint8_t tms, uint8_t tdi, uint8_t* tdo)
{
	if (cycles > 64 || cycles == 0)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	if (tms != 0 && tms != 1)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	uint8_t in[8];
	for (uint32_t i = 0; i < 8; i++)
		in[i] = tdi;
	
	SequenceInfo info = { 0 };
	info.cycles = ((cycles == 64) ? 0 : cycles);
	info.TMS = tms ? 1 : 0;
	info.TDO = tdo ? 1 : 0;
	return cmdJtagSequence(info, in, tdo);
}

int32_t CMSISDAP::cmdJtagConfigure(const std::vector<uint8_t>& irLength)
{
	if (irLength.size() < 1 || irLength.size() > 60)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_JTAG_CONFIGURE);
	tx.write((uint8_t)irLength.size());
	for (auto len : irLength)
	{
		tx.write(len);
	}

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	uint8_t* rxdata = rx.data();
	if (rxdata[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdJtagSequence(SequenceInfo info, uint8_t* in, uint8_t* out)
{
	if (in == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_JTAG_SEQ);
	tx.write(1);
	tx.write(info.raw[0]);

	uint32_t bytes = (info.cycles == 0) ? 8 : (((info.cycles - 1) >> 3) + 1);
	for (uint32_t i = 0; i < bytes; i++)
	{
		tx.write(*in);
		in++;
	}

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	uint8_t* rxdata = rx.data();
	if (rxdata[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}
	else if (out)
	{
		for (uint32_t i = 0; i < bytes; i++)
		{
			*out = rxdata[i + 2];
			out++;
		}
	}

	return ret;
}

int32_t CMSISDAP::initialize(void)
{
	int32_t ret;
	ret = cmdInfoCapabilities();
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdLed(RUNNING, false);
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdLed(CONNECT, false);
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdLed(CONNECT, true);
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdInfoFwVer();
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdInfoVendor();
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdInfoName();
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdInfoPacketSize();
	if (ret != OK) {
		return ret;
	}

	ret = cmdInfoPacketCount();
	if (ret != OK) {
		return ret;
	}

	ret = getPinStatus();
	if (ret != OK) {
		return ret;
	}

	ret = cmdSwjClock(1000 * 1000); /* 1000kHz */
	if (ret != OK) {
		return ret;
	}

	ret = cmdTxConf(0, 64, 0);
	if (ret != OK) {
		return ret;
	}

	ret = cmdLed(RUNNING, true);
	if (ret != OK) {
		return ret;
	}

	_DBGPRT("Init OK.\n");
	_DBGPRT("  F/W Version : %s\n", fwver == "" ? "none" : fwver.c_str());
	_DBGPRT("  Packet Size : %u\n", packetMaxSize);
	_DBGPRT("  Packet Cnt  : %u\n", packetMaxCount);
	_DBGPRT("  Capabilities: %s (0x%08x)\n",
		caps.SWD && caps.JTAG ? "JTAG/SWD" :
		caps.JTAG ? "JTAG" :
		caps.SWD ? "SWD" : "UNKNOWN", caps.raw);
	_DBGPRT("  Vendor Name : %s\n", vendor == "" ? "none" : vendor.c_str());
	_DBGPRT("  Name        : %s\n", name == "" ? "none" : name.c_str());
	_DBGPRT("  USB PID     : 0x%04x\n", pid);
	_DBGPRT("  USB VID     : 0x%04x\n", vid);

	return ret;
}

int32_t CMSISDAP::setConnectionType(ConnectionType type)
{
	int32_t ret;
	
	if (type == JTAG)
	{
		ret = cmdConnect(DAP_MODE_JTAG);
		if (ret != OK)
			return ret;

		connectionType = JTAG;
	}
	else if (type == SWJ_JTAG)
	{
		ret = cmdConnect(DAP_MODE_JTAG);
		if (ret != OK)
			return ret;

		// magic packetを送り、JTAG へ移行
		ret = swdToJtag();
		if (ret != OK)
		{
			_ERRPRT("Failed to switch to JTAG mode. (0x%08x)\n", ret);
			return ret;
		}

		connectionType = SWJ_JTAG;
	}
	else if (type == SWJ_SWD)
	{
		ret = cmdConnect(DAP_MODE_SWD);
		if (ret != OK)
			return ret;

		ret = cmdSwdConf(0x00);
		if (ret != OK)
			return ret;

		// magic packetを送り、SWD へ移行
		ret = jtagToSwd();
		if (ret != OK)
		{
			_ERRPRT("Failed to switch to SWD mode. (0x%08x)\n", ret);
			return ret;
		}

		connectionType = SWJ_SWD;
	}

	return OK;
}

int32_t CMSISDAP::setSpeed(uint32_t speed)
{
	if (speed > _CMSISDAP_MAX_CLOCK) {
		speed = _CMSISDAP_MAX_CLOCK;
	}

	return cmdSwjClock(speed);
}

int32_t CMSISDAP::dpRead(uint32_t reg, uint32_t *data)
{
	return dpapRead(true, reg, data);
}

int32_t CMSISDAP::dpWrite(uint32_t reg, uint32_t val)
{
	return dpapWrite(true, reg, val);
}

int32_t CMSISDAP::apRead(uint32_t reg, uint32_t *data)
{
	return dpapRead(false, reg, data);
}

int32_t CMSISDAP::apWrite(uint32_t reg, uint32_t val)
{
	return dpapWrite(false, reg, val);
}

int32_t CMSISDAP::dpapRead(bool dp, uint32_t reg, uint32_t *data)
{
	TransferRequest req = { 0 };

	req.setRead();
	if (dp)
		req.setDP();
	else
		req.setAP();
	req.setRegister(reg);

	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_TX);
	tx.write(dapIndex);	/* DAP Index, ignored in the swd. */
	tx.write(0x01);	/* Tx count */
	tx.write(req.raw[0]);
	tx.write32(0);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
		return ret;

	uint8_t* rxdata = rx.data();

	if (rxdata[1] == 0)
		return CMSISDAP_ERR_ACKFAULT;

	switch (rxdata[2] & TX_ACK_MASK)
	{
	case TX_ACK_NO_ACK:
		return CMSISDAP_ERR_NO_ACK;
	case TX_ACK_FAULT:
		return CMSISDAP_ERR_ACKFAULT;
	case TX_ACK_WAIT:
		return CMSISDAP_ERR_ACKWAIT;
	}

	if (data != NULL)
		*data = buf2LE32(&rxdata[3]);

	return OK;
}

int32_t CMSISDAP::dpapWrite(bool dp, uint32_t reg, uint32_t data)
{
	TransferRequest req = { 0 };

	req.setWrite();
	if (dp)
		req.setDP();
	else
		req.setAP();
	req.setRegister(reg);

	TxPacket tx;
	tx.write(_USB_HID_REPORT_NUM);
	tx.write(CMD_TX);
	tx.write(dapIndex);	/* DAP Index, ignored in the swd. */
	tx.write(0x01);	/* Tx count */
	tx.write(req.raw[0]);
	tx.write32(data);

	RxPacket rx;
	int ret = usbTxRx(tx, &rx);
	if (ret != OK)
		return ret;

	uint8_t* rxdata = rx.data();
	if (rxdata[1] != 0x01) {
		return rxdata[2];
	}
	return OK;
}

