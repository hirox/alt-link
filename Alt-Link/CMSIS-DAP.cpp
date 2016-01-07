
#include "stdafx.h"

#include "CMSIS-DAP.h"
#include "ADIv5.h"

#define USE_USB_TX_DBG 0

/*
 * based on CMSIS-DAP Beta 0.01.
 * https://silver.arm.com/browse/CMSISDAP
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

/* CMD_CONNECT */
enum ConnectInterface : uint8_t
{
	DEFAULT	= 0x0,
	SWD		= 0x1,
	JTAG	= 0x2
};

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

#define CMSIS_CMD_DP (0 << 0)         /* set only for AP access */
#define CMSIS_CMD_AP (1 << 0)         /* set only for AP access */
#define CMSIS_CMD_READ (1 << 1)       /* set only for read access */
#define CMSIS_CMD_WRITE (0 << 1)      /* set only for read access */
#define CMSIS_CMD_A32(n) ((n) & 0x0C) /* bits A[3:2] of register addr */
#define CMSIS_CMD_VAL_MATCH (1 << 4)  /* Value Match */
#define CMSIS_CMD_MATCH_MSK (1 << 5)  /* Match Mask */

/* three-bit ACK values for SWD access (sent LSB first) */
#define TX_ACK_OK 0x1
#define TX_ACK_WAIT 0x2
#define TX_ACK_FAULT 0x4

#define WCR_TO_TRN(wcr) ((uint32_t)(1 + (3 & ((wcr)) >> 8))) /* 1..4 clocks */
#define WCR_TO_PRESCALE(wcr) ((uint32_t)(7 & ((wcr))))       /* impl defined */

int32_t CMSISDAP::usbOpen(void)
{
	struct hid_device_info *info;
	struct hid_device_info *infoCur;
	uint16_t usbVID = 0;
	uint16_t usbPID = 0;

	packetBufSize = _CMSISDAP_DEFAULT_PACKET_SIZE;
	infoCur = info = hid_enumerate(0x0, 0x0);
	while (NULL != infoCur) {
#if 0
		printf("type: %04hx %04hx\npath: %s\nserial_number: %ls\n", infoCur->vendor_id, infoCur->product_id, infoCur->path, infoCur->serial_number);
		printf("Manu    : %ls\n", infoCur->manufacturer_string);
		printf("Product : %ls\n", infoCur->product_string);
#endif
		if (infoCur->product_string != NULL &&
			wcsstr(infoCur->product_string, L"CMSIS-DAP"))
		{
			break;
		}
		infoCur = infoCur->next;
	}

	if (infoCur != NULL)
	{
		usbVID = infoCur->vendor_id;
		usbPID = infoCur->product_id;
	}

	if (info != NULL)
	{
		hid_free_enumeration(info);
	}

	if (usbVID == 0 && usbPID == 0)
	{
		return CMSISDAP_ERR_USBHID_NOT_FOUND_DEVICE;
	}

	if (hid_init() != 0)
	{
		return CMSISDAP_ERR_USBHID_INIT;
	}

	hidHandle = hid_open(usbVID, usbPID, NULL);
	if (hidHandle == NULL)
	{
		return CMSISDAP_ERR_USBHID_OPEN;
	}

	packetBufSize = packetBufSize;
	vid = usbVID;
	pid = usbPID;

	packetBuf = (uint8_t *)malloc(packetBufSize);
	if (packetBuf == NULL)
	{
		return CMSISDAP_ERR_NO_MEMORY;
	}

	return OK;
}

int32_t CMSISDAP::usbClose(void)
{
	(void)hid_close(hidHandle);
	if (hid_exit() != 0)
	{
		return CMSISDAP_ERR_USBHID_EXIT;
	}
	return OK;
}

int32_t CMSISDAP::usbTx(uint32_t txlen)
{
	int ret;
#if USE_USB_TX_DBG
	uint8_t *buf = packetBuf;
#endif /* USE_USB_TX_DBG */

	if (packetBufSize - 1u < txlen) {
		return CMSISDAP_ERR_INVALID_TX_LEN;
	}

	memset(packetBuf + txlen, 0, packetBufSize - 1 - txlen);

#if USE_USB_TX_DBG
	_DBGPRT("A %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
#endif /* USE_USB_TX_DBG */
	ret = hid_write(hidHandle, packetBuf, packetBufSize);
	if (ret == -1) {
		return CMSISDAP_ERR_USBHID_WRITE;
	}

	ret = hid_read_timeout(hidHandle, packetBuf, packetBufSize, _CMSISDAP_USB_TIMEOUT);
	if (ret == -1 || ret == 0) {
		return CMSISDAP_ERR_USBHID_TIMEOUT;
	}
#if USE_USB_TX_DBG
	_DBGPRT("B %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
#endif /* USE_USB_TX_DBG */

	return OK;
}

int32_t CMSISDAP::cmdInfoCapabilities(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_INFO;
	packetBuf[idx++] = INFO_ID_CAPABILITIES;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] != 1) {
		return CMSISDAP_ERR_DAP_RES;
	}

	caps.raw = packetBuf[2];

	return OK;
}

int32_t CMSISDAP::cmdLed(LED led, bool on)
{
	int ret;
	uint32_t idx = 0;
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_LED;
	packetBuf[idx++] = led;
	packetBuf[idx++] = on ? 1 : 0;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}
	if (packetBuf[1] != _DAP_RES_OK) {
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdConnect(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_CONNECT;
	packetBuf[idx++] = SWD;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] != SWD) {
		return CMSISDAP_ERR_FATAL;
	}

	return OK;
}

int32_t CMSISDAP::cmdDisconnect(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_DISCONNECT;
	ret = usbTx(idx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (packetBuf[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdTxConf(uint8_t idle, uint16_t delay, uint16_t retry)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_TX_CONF;
	packetBuf[idx++] = idle;
	packetBuf[idx++] = delay & 0xff;
	packetBuf[idx++] = (delay >> 8) & 0xff;
	packetBuf[idx++] = retry & 0xff;
	packetBuf[idx++] = (retry >> 8) & 0xff;
	ret = usbTx(idx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (packetBuf[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdInfoFwVer(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_INFO;
	packetBuf[idx++] = INFO_ID_FW_VER;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] > 0) {
		fwver = std::string((const char *)&(packetBuf[2]));
	} else {
		fwver = "";
	}

	return OK;
}

int32_t CMSISDAP::cmdInfoVendor(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_INFO;
	packetBuf[idx++] = INFO_ID_TD_VEND;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] > 0)
		vendor = std::string((const char *)&(packetBuf[2]));
   	else
		vendor = "";

	return OK;
}

int32_t CMSISDAP::cmdInfoName(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_INFO;
	packetBuf[idx++] = INFO_ID_TD_NAME;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] > 0)
		name = std::string((const char *)&(packetBuf[2]));
	else
		name = "";

	return OK;
}

int32_t CMSISDAP::cmdInfoPacketSize(void)
{
	int ret;
	uint16_t size;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_INFO;
	packetBuf[idx++] = INFO_ID_PKT_SZ;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] != 2) {
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	size = packetBuf[2] + (packetBuf[3] << 8);

	if (packetBufSize != size + 1) {
		// 現在のバッファサイズと異なる場合、realloc
		uint8_t *tmp;
		/* reallocate buffer */
		packetBufSize = size + 1;
		tmp = (uint8_t*)realloc(packetBuf, packetBufSize);
		if (tmp == NULL) {
			free(packetBuf);
			return CMSISDAP_ERR_NO_MEMORY;
		}
		packetBuf = tmp;
	}
	return OK;
}

int32_t CMSISDAP::cmdInfoPacketCount(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_INFO;
	packetBuf[idx++] = INFO_ID_PKT_CNT;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	if (packetBuf[1] != 1) { /* resは1byte */
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	packetMaxCount = packetBuf[2];
	return OK;
}

int32_t CMSISDAP::getStatus(void)
{
	int ret;
	uint8_t d;

	ret = cmdSwjPins(0, _PIN_SWCLK, 0, &d);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	_DBGPRT("SWCLK:%d SWDIO:%d TDI:%d TDO:%d !TRST:%d !RESET:%d\n", (d & _PIN_SWCLK) ? 1 : 0, (d & _PIN_SWDIO) ? 1 : 0,
	        (d & _PIN_TDI) ? 1 : 0, (d & _PIN_TDO) ? 1 : 0, (d & _PIN_nTRST) ? 1 : 0, (d & _PIN_nRESET) ? 1 : 0);

	return OK;
}

int32_t CMSISDAP::jtagToSwd(void)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWJ_SEQ;
	packetBuf[idx++] = 7 * 8;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	idx = 0;
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWJ_SEQ;
	packetBuf[idx++] = 2 * 8;
	packetBuf[idx++] = 0x9e;
	packetBuf[idx++] = 0xe7;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	idx = 0;
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWJ_SEQ;
	packetBuf[idx++] = 7 * 8;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	packetBuf[idx++] = 0xff;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	/* 8 cycle idle period */
	idx = 0;
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWJ_SEQ;
	packetBuf[idx++] = 8;
	packetBuf[idx++] = 0x00;
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}

	return ret;
}

int32_t CMSISDAP::resetLink(void)
{
	int ret;
	uint32_t idx = 0;

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

	idx = 0;
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_WRITE_ABORT;
	packetBuf[idx++] = 0x00; /* DAP Index, ignored in the swd. */
	packetBuf[idx++] = AP_ABORT_STK_CMP_CLR | AP_ABORT_WD_ERR_CLR | AP_ABORT_ORUN_ERR_CLR | AP_ABORT_STK_ERR_CLR;
	packetBuf[idx++] = 0x00; /* SBZ */
	packetBuf[idx++] = 0x00; /* SBZ */
	packetBuf[idx++] = 0x00; /* SBZ */
	ret = usbTx(idx);
	if (ret != OK) {
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	if (packetBuf[1] != _DAP_RES_OK) {
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdSwjPins(uint8_t isLevelHigh, uint8_t pin, uint32_t delay, uint8_t *input)
{
	int ret;
	uint32_t idx = 0;
	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWJ_PINS;
	packetBuf[idx++] = !!(isLevelHigh);
	packetBuf[idx++] = pin;
	packetBuf[idx++] = delay & 0xff;
	packetBuf[idx++] = (delay >> 8) & 0xff;
	packetBuf[idx++] = (delay >> 16) & 0xff;
	packetBuf[idx++] = (delay >> 24) & 0xff;
	ret = usbTx(idx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (input != NULL)
	{
		*input = packetBuf[1];
	}

	return ret;
}

int32_t CMSISDAP::cmdSwjClock(uint32_t clock)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWJ_CLOCK;
	packetBuf[idx++] = clock & 0xff;
	packetBuf[idx++] = (clock >> 8) & 0xff;
	packetBuf[idx++] = (clock >> 16) & 0xff;
	packetBuf[idx++] = (clock >> 24) & 0xff;
	ret = usbTx(idx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (packetBuf[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::cmdSwdConf(uint8_t cfg)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_SWD_CONF;
	packetBuf[idx++] = cfg;
	ret = usbTx(idx);
	if (ret != OK)
	{
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
		return ret;
	}
	if (packetBuf[1] != _DAP_RES_OK)
	{
		ret = CMSISDAP_ERR_DAP_RES;
		_DBGPRT("err ret=%08x %s %s %d\n", ret, __FUNCTION__, __FILE__, __LINE__);
	}

	return ret;
}

int32_t CMSISDAP::initialize(void)
{
	int32_t ret;
	ret = usbOpen();
	if (ret != OK)
	{
		return ret;
	}

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

	ret = cmdConnect();
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

	ret = getStatus();
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
	ret = cmdSwdConf(0x00);
	if (ret != OK) {
		return ret;
	}

	ret = cmdLed(RUNNING, true);
	if (ret != OK) {
		return ret;
	}

	_DBGPRT("Init OK.\n");
	_DBGPRT("  F/W Version : %s\n", fwver == "" ? "none" : fwver.c_str());
	_DBGPRT("  Packet Size : %u\n", packetBufSize);
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

int32_t CMSISDAP::finalize(void)
{
	int ret;

	ret = cmdDisconnect();
	if (ret != OK)
	{
		return ret;
	}

	ret = cmdLed(RUNNING, 0);
	if (ret != OK) {
		return ret;
	}

	ret = cmdLed(CONNECT, 0);
	if (ret != OK) {
		return ret;
	}

	if (packetBuf != NULL) {
		free(packetBuf);
	}

	return ret;
}

int32_t CMSISDAP::resetSw(void)
{
	return cmdSwjPins(_PIN_SWCLK, 1, 0, NULL);
}

int32_t CMSISDAP::resetHw(void)
{
	return cmdSwjPins(_PIN_nRESET, 1, 0, NULL);
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
	int ret;
	uint32_t val;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM;
	packetBuf[idx++] = CMD_TX;
	packetBuf[idx++] = 0x00; /* DAP Index, ignored in the swd. */
	packetBuf[idx++] = 0x01; /* Tx count */
	packetBuf[idx++] = (dp ? CMSIS_CMD_DP : CMSIS_CMD_AP) | CMSIS_CMD_READ | CMSIS_CMD_A32(reg);
	ret = usbTx(idx);
	if (ret != OK) {
		return ret;
	}
	if ((packetBuf[2] & TX_ACK_FAULT) != 0) {
		ret = CMSISDAP_ERR_ACKFAULT;
		return ret;
	}

	val = buf2LE32(&packetBuf[3]);

	if (data != NULL) {
		*data = val;
	}

	return OK;
}

int32_t CMSISDAP::dpapWrite(bool dp, uint32_t reg, uint32_t data)
{
	int ret;
	uint32_t idx = 0;

	packetBuf[idx++] = _USB_HID_REPORT_NUM; /* report number */
	packetBuf[idx++] = CMD_TX;
	packetBuf[idx++] = 0x00;
	packetBuf[idx++] = 0x01;
	packetBuf[idx++] = (dp ? CMSIS_CMD_DP : CMSIS_CMD_AP) | CMSIS_CMD_WRITE | CMSIS_CMD_A32(reg);
	packetBuf[idx++] = (data)& 0xff;
	packetBuf[idx++] = (data >> 8) & 0xff;
	packetBuf[idx++] = (data >> 16) & 0xff;
	packetBuf[idx++] = (data >> 24) & 0xff;
	ret = usbTx(idx);

	if (packetBuf[1] != 0x01) {
		ret = packetBuf[2];
	}
	return OK;
}

