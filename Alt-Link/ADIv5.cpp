
#include "stdafx.h"
#include "ADIv5.h"
#include "JEP106.h"

#include <thread>
#include <chrono>

#define AP_ABORT_DAPABORT 0x01     /* generate a DAP abort */
#define AP_ABORT_STK_CMP_CLR 0x02  /* clear STICKYCMP sticky compare flag */
#define AP_ABORT_STK_ERR_CLR 0x04  /* clear STICKYERR sticky error flag */
#define AP_ABORT_WD_ERR_CLR 0x08   /* clear WDATAERR write data error flag */
#define AP_ABORT_ORUN_ERR_CLR 0x10 /* clear STICKYORUN overrun error flag */

#define BANK_REG(bank, reg) (((bank) << 4) | (reg))

/* A[3:0] for DP registers; A[1:0] are always zero.
 * - JTAG accesses all of these via JTAG_DP_DPACC, except for
 *   IDCODE (JTAG_DP_IDCODE) and ABORT (JTAG_DP_ABORT).
 * - SWD accesses these directly, sometimes needing SELECT.CTRLSEL
 */
#define DP_REG_IDCODE		BANK_REG(0x0, 0x0)	/* SWD: read / DPIDR */
#define DP_REG_ABORT		BANK_REG(0x0, 0x0)	/* SWD: write */
#define DP_REG_CTRL_STAT	BANK_REG(0x0, 0x4)	/* r/w */
#define DP_RESEND			BANK_REG(0x0, 0x8)	/* SWD: read */
#define DP_REG_SELECT		BANK_REG(0x0, 0x8)	/* JTAG: r/w; SWD: write */
#define DP_RDBUFF			BANK_REG(0x0, 0xC)	/* read-only */
#define DP_WCR				BANK_REG(0x1, 0x4)	/* SWD: r/w */

#define WCR_TO_TRN(wcr) ((uint32_t)(1 + (3 & ((wcr)) >> 8))) /* 1..4 clocks */
#define WCR_TO_PRESCALE(wcr) ((uint32_t)(7 & ((wcr))))       /* impl defined */

#if 0
/* Fields of the DP's CTRL/STAT register */
#define CORUNDETECT (1UL << 0)
#define SSTICKYORUN (1UL << 1)
/* 3:2 - transaction mode (e.g. pushed compare) */
#define SSTICKYCMP (1UL << 4)
#define SSTICKYERR (1UL << 5)
#define READOK (1UL << 6)   /* SWD-only */
#define WDATAERR (1UL << 7) /* SWD-only */
/* 11:8 - mask lanes for pushed compare or verify ops */
/* 21:12 - transaction counter */
#define CDBGRSTREQ (1UL << 26)
#define CDBGRSTACK (1UL << 27)
#define CDBGPWRUPREQ (1UL << 28)
#define CDBGPWRUPACK (1UL << 29)
#define CSYSPWRUPREQ (1UL << 30)
#define CSYSPWRUPACK (1UL << 31)
#endif

union DP_ABORT
{
	struct
	{
		/* Fields of the DP's AP ABORT register */
		uint32_t DAPABORT	: 1;
		uint32_t STKCMPCLR	: 1;	/* SWD-only */
		uint32_t STKERRCLR	: 1;	/* SWD-only */
		uint32_t WDERRCLR	: 1;	/* SWD-only */
		uint32_t ORUNERRCLR	: 1;	/* SWD-only */
		uint32_t Reserved	: 27;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DP_ABORT));

union DP_SELECT
{
	struct
	{
		uint8_t DPBANKSEL : 4;
		uint8_t APBANKSEL : 4;
		uint8_t Reserved[2];
		uint8_t APSEL;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(DP_SELECT));

union AP_IDR
{
	enum ApClass : uint32_t
	{
		NoDefined			= 0x0,
		MemoryAccessPort	= 0x8
	};

	struct
	{
		uint32_t Type		: 4;
		uint32_t Variant	: 4;
		uint32_t Reserved	: 5;
		ApClass Class		: 4;
		uint32_t IdentityCode		: 7;
		uint32_t ContinuationCode	: 4;
		uint32_t Revision	:4;
	};
	uint32_t raw;
	bool isAHB() { return (Type == 0x01 && Class == MemoryAccessPort) ? true : false; }
	bool isAPB() { return (Type == 0x02 && Class == MemoryAccessPort) ? true : false; }
	bool isAXI() { return (Type == 0x04 && Class == MemoryAccessPort) ? true : false; }
};
static_assert(CONFIRM_UINT32(AP_IDR));

union MEM_AP_CSW
{
	struct
	{
		ADIv5::MEM_AP::AccessSize Size	: 3;
		uint32_t Reseved0		: 1;
		uint32_t AddrInc		: 2;
		uint32_t DeviceEn		: 1;
		uint32_t TrInProg		: 1;
		uint32_t Mode			: 4;
		uint32_t Type			: 4;
		uint32_t Reserved1		: 7;
		uint32_t SPIDEN			: 1;
		uint32_t Prot			: 5;
		uint32_t Reserved2		: 1;
		uint32_t SProt			: 1;
		uint32_t DbgSwEnable	: 1;
	};
	uint32_t raw;
	void print();
};
static_assert(CONFIRM_UINT32(MEM_AP_CSW));

union AHB_AP_CSW
{
	struct
	{
		ADIv5::MEM_AP::AccessSize Size	: 3;
		uint32_t Reseved0		: 1;
		uint32_t AddrInc		: 2;
		uint32_t DeviceEn		: 1;
		uint32_t TrInProg		: 1;
		uint32_t Mode			: 4;
		uint32_t Reserved1		: 13;
		uint32_t Hprot1			: 1;
		uint32_t Reserved2		: 3;
		uint32_t MasterType		: 1;
		uint32_t Reserved3		: 2;
	};
	uint32_t raw;
	void print();
};
static_assert(CONFIRM_UINT32(AHB_AP_CSW));

union MEM_AP_CFG
{
	struct
	{
		uint32_t BE			: 1;	// Big Endian
		uint32_t LA			: 1;	// Long address (Large Physical Address)
		uint32_t LD			: 1;	// Large data
		uint32_t Reserved	: 29;
	};
	uint32_t raw;
};
static_assert(CONFIRM_UINT32(MEM_AP_CFG));

/* MEM-AP register addresses */
/* TODO: rename as MEM_AP_REG_* */
#define MEM_AP_REG_CSW		0x00
#define MEM_AP_REG_TAR		0x04
#define MEM_AP_REG_TAR2		0x08	// for 64bit
#define MEM_AP_REG_DRW		0x0C
#define MEM_AP_REG_BD0 0x10
#define MEM_AP_REG_BD1 0x14
#define MEM_AP_REG_BD2 0x18
#define MEM_AP_REG_BD3 0x1C
#define MEM_AP_REG_CFG		0xF4
#define MEM_AP_REG_BASE2	0xF0	// for 64bit
#define MEM_AP_REG_BASE		0xF8

/* Generic AP register address */
#define AP_REG_IDR 0xFC

/* Fields of the MEM-AP's CSW register */
#define CSW_8BIT 0
#define CSW_16BIT 1
#define CSW_32BIT 2
#define CSW_ADDRINC_MASK (3UL << 4)
#define CSW_ADDRINC_OFF 0UL
#define CSW_ADDRINC_SINGLE (1UL << 4)
#define CSW_ADDRINC_PACKED (2UL << 4)
#define CSW_DEVICE_EN (1UL << 6)
#define CSW_TRIN_PROG (1UL << 7)
#define CSW_SPIDEN (1UL << 23)
/* 30:24 - implementation-defined! */
#define CSW_HPROT (1UL << 25)        /* ? */
#define CSW_MASTER_DEBUG (1UL << 29) /* ? */
#define CSW_SPROT (1UL << 30)
#define CSW_DBGSWENABLE (1UL << 31)

int32_t ADIv5::getIDCODE(DP_IDCODE* idcode)
{
	if (idcode == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	return dap.dpRead(DP_REG_IDCODE, &idcode->raw);
}

void ADIv5::DP_IDCODE::print()
{
	_DBGPRT("  IDCODE      : 0x%08x\n", raw);
	if (RAO)
	{
		_DBGPRT("    MANUFACTURER : %s\n", DESIGNER == 0x23B ? "ARM" : "UNKNOWN");
		if (PARTNO == 0xBA && RES0 == 0 && MIN == 0 && VERSION == 0)
		{
			_DBGPRT("    PARTSNO      : JTAG-DP\n");
			_DBGPRT("    VERSION      : %x\n", REVISION);
		}
		else
		{
			_DBGPRT("    PARTSNO      : %s\n",
				(PARTNO == 0xBA) ? "SW-DP" :
				(PARTNO == 0xBB) ? "SW-DP (M0)" :
				(PARTNO == 0xBC) ? "SW-DP (M0+)" : "UNKNOWN");
			_DBGPRT("    VERSION      : v%x\n", VERSION);
			_DBGPRT("    MIN          : %s\n", MIN ? "MINDP" : "No");
			_DBGPRT("    REVISION     : %x\n", REVISION);
		}
	}
	else
	{
		_DBGPRT("    Invalid Format\n");
	}
}

int32_t ADIv5::getCtrlStat(DP_CTRL_STAT* ctrlStat)
{
	if (ctrlStat == nullptr)
		return CMSISDAP_ERR_INVALID_ARGUMENT;

	return dap.dpRead(DP_REG_CTRL_STAT, &ctrlStat->raw);
}

int32_t ADIv5::setCtrlStat(DP_CTRL_STAT& ctrlStat)
{
	return dap.dpWrite(DP_REG_CTRL_STAT, ctrlStat.raw);
}

void ADIv5::DP_CTRL_STAT::print()
{
	_DBGPRT("  CTRL/STAT   : 0x%08x\n", raw);
	_DBGPRT("    SYS PWR UP REQ: %x ACK: %x\n", CSYSPWRUPREQ, CSYSPWRUPACK);
	_DBGPRT("    DBG PWR UP REQ: %x ACK: %x RST REQ: %x RST ACK:%x\n",
		CDBGPWRUPREQ, CDBGPWRUPACK, CDBGRSTREQ, CDBGRSTACK);
	_DBGPRT("    Transaction Counter: 0x%04x MASKLANE: 0x%02x\n", TRNCNT, MASKLANE);
	_DBGPRT("    STICKYERR: %x STICKYCMP: %x STICKYORUN: %x TRNMODE: %x ORUNDETECT: %x\n",
		STICKYERR, STICKYCMP, STICKYORUN, TRNMODE, ORUNDETECT);
	_DBGPRT("    (SWD) Write Data Error: %x Read OK: %x\n", WDATAERR, READOK);
}

int32_t ADIv5::powerupDebug()
{
	DP_CTRL_STAT ctrlStat;
	int32_t ret = getCtrlStat(&ctrlStat);
	if (ret != OK)
		return ret;

	ctrlStat.print();

	if (ctrlStat.STICKYERR || ctrlStat.WDATAERR)
	{
		ret = clearError();
		if (ret != OK)
			return ret;
	}

	// CDBGPWRUPACK が立っていない場合は電源を入れる
	if (!ctrlStat.CDBGPWRUPACK)
	{
		// JTAG-DP では W1C (Write a 1 clears to 0)
		ctrlStat.STICKYCMP = 0;
		ctrlStat.STICKYERR = 0;
		ctrlStat.STICKYORUN = 0;

		ctrlStat.CDBGPWRUPREQ = 1;
		ret = setCtrlStat(ctrlStat);
		if (ret != OK)
			return ret;

		while (1)
		{
			ret = getCtrlStat(&ctrlStat);
			if (ret != OK)
				return ret;

			if (ctrlStat.CDBGPWRUPACK)
				break;

			// [TODO] timeout

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		_DBGPRT("DBG Power up\n");
		ctrlStat.print();
	}
	return OK;
}

void MEM_AP_CSW::print()
{
	_DBGPRT("    Control/Status    : 0x%08x\n", raw);
	_DBGPRT("      Device          : %s\n", DeviceEn ? "enabled" : "disabled");
	if (DeviceEn)
		_DBGPRT("      Debug SW Access : %s\n", DbgSwEnable ? "enabled" : "disabled");
	_DBGPRT("      Secure Access   : %s\n", SPIDEN ? "enabled" : "disabled");
	_DBGPRT("      SProt/Prot/Type : %x/%x/%x\n", SProt, Prot, Type);
	_DBGPRT("      Mode            : %s\n",
		Mode == 0 ? "basic" :
		Mode == 1 ? "barrier support enabled" : "UNKNOWN");
	_DBGPRT("      Transfer        : %s\n", TrInProg ? "in progress" : "idle");
	_DBGPRT("      Auto increment  : %s\n",
		AddrInc == 0 ? "off" :
		AddrInc == 1 ? "single" :
		AddrInc == 2 ? "packed" : "UNKNOWN");
	_DBGPRT("      Size            : %s\n",
		Size == ADIv5::MEM_AP::SIZE_8BIT ? "8bit" :
		Size == ADIv5::MEM_AP::SIZE_16BIT ? "16bit" :
		Size == ADIv5::MEM_AP::SIZE_32BIT ? "32bit" :
		Size == ADIv5::MEM_AP::SIZE_64BIT ? "64bit" :
		Size == ADIv5::MEM_AP::SIZE_128BIT ? "128bit" :
		Size == ADIv5::MEM_AP::SIZE_256BIT ? "256bit" : "UNKNOWN");
}

void AHB_AP_CSW::print()
{
	_DBGPRT("    Control/Status    : 0x%08x\n", raw);
	_DBGPRT("      Device          : %s\n", DeviceEn ? "enabled" : "disabled");
	_DBGPRT("      Master          : %s\n", MasterType ? "debug" : "core [!]");
	_DBGPRT("      Hprot1          : %s\n", Hprot1 ? "privileged" : "user [!]");
	_DBGPRT("      Mode            : %s\n",
		Mode == 0 ? "basic" :
		Mode == 1 ? "barrier support enabled" : "UNKNOWN");
	_DBGPRT("      Transfer        : %s\n", TrInProg ? "in progress" : "idle");
	_DBGPRT("      Auto increment  : %s\n",
		AddrInc == 0 ? "off" :
		AddrInc == 1 ? "single" :
		AddrInc == 2 ? "packed" : "UNKNOWN");
	_DBGPRT("      Size            : %s\n",
		Size == ADIv5::MEM_AP::SIZE_8BIT ? "8bit" :
		Size == ADIv5::MEM_AP::SIZE_16BIT ? "16bit" :
		Size == ADIv5::MEM_AP::SIZE_32BIT ? "32bit" :
		Size == ADIv5::MEM_AP::SIZE_64BIT ? "64bit" :
		Size == ADIv5::MEM_AP::SIZE_128BIT ? "128bit" :
		Size == ADIv5::MEM_AP::SIZE_256BIT ? "256bit" : "UNKNOWN");
}

int32_t ADIv5::scanAPs()
{
	_DBGPRT("AP SCAN\n");

	for (uint32_t i = 0; i < 255; i++)
	{
		AP_IDR idr;
		int32_t ret = ap.read(i, AP_REG_IDR, &idr.raw);
		if (ret != OK) {
			return ret;
		}

		if (idr.raw == 0)
			break;

		_DBGPRT("  AP-%d\n", i);
		_DBGPRT("    IDR: 0x%08x\n", idr.raw);
		_DBGPRT("      Designer   : %s\n", getJEP106DesignerName(idr.ContinuationCode, idr.IdentityCode));
		_DBGPRT("      Class/Type : %s\n",
			idr.Type == 0x00 && idr.Class == AP_IDR::NoDefined ? "JTAG-AP" :
			idr.isAHB() ? "MEM-AP AMBA AHB bus" :
			idr.isAPB() ? "MEM-AP AMBA APB2 or APB3 bus" :
			idr.isAXI() ? "MEM-AP AMBA AXI4 ot AXI4 bus" : "UNKNOWN");
		_DBGPRT("      Variant    : %x\n", idr.Variant);
		_DBGPRT("      Revision   : %x\n", idr.Revision);

		if (idr.Class == AP_IDR::MemoryAccessPort)
		{
			bool hasDebugEntry = false;
			uint32_t base;
			ret = ap.read(i, MEM_AP_REG_BASE, &base);
			if (ret != OK) {
				return ret;
			}
			_DBGPRT("    BASE : 0x%08x (%x)\n", base & 0xFFFFF000, base);
			if (base == 0xFFFFFFFF)
			{
				_DBGPRT("      Debug entry : no (legacy format)\n");
			}
			else if ((base & 0x02) == 0)
			{
				_DBGPRT("      Debug entry : present (legacy format)\n");
				hasDebugEntry = true;
			}
			else
			{
				_DBGPRT("      Debug entry : %s\n", base & 0x1 ? "present" : "no");
				hasDebugEntry = (base & 0x1) ? true : false;
			}

			if (idr.isAHB())
			{
				AHB_AP_CSW csw;
				ret = ap.read(i, MEM_AP_REG_CSW, &csw.raw);
				if (ret != OK) {
					return ret;
				}
				csw.print();
			}
			else
			{
				MEM_AP_CSW csw;
				ret = ap.read(i, MEM_AP_REG_CSW, &csw.raw);
				if (ret != OK) {
					return ret;
				}
				csw.print();
			}

			if (!hasDebugEntry)
			{
				// Debug Entry なしの AHB bus は sysmem なので登録する
				if (idr.isAHB())
					ahbSysmemAps.push_back(std::make_shared<MEM_AP>(i, ap));
				continue;
			}

			std::shared_ptr<MEM_AP> memAp = std::make_shared<MEM_AP>(i, ap);
			std::shared_ptr<Component> component = std::make_shared<Component>(Memory(*memAp, base & 0xFFFFF000));

			ret = component->readPidCid();
			if (ret != OK)
			{
				_ERRPRT("Failed to read ROM Table\n");
				return ret;
			}

			if (component->isRomTable())
			{
				memAps.push_back(std::make_pair(memAp, ROM_TABLE(component)));
				memAps.back().second.read();
			}
			else
			{
				_DBGPRT("ROM Table was not found\n");
			}

#if 0
			if (i == 1)
			{
				for (auto j : {0, 1, 2, 3, 4, 5})
				{
					uint32_t pc;
					memAp->read(0xf7b30084, &pc);
					_DBGPRT("CPU0 PC: 0x%08x\n", pc);
					memAp->read(0xf7b32084, &pc);
					_DBGPRT("CPU1  PC: 0x%08x\n", pc);
					memAp->read(0xf7b34084, &pc);
					_DBGPRT("CPU2   PC: 0x%08x\n", pc);
					memAp->read(0xf7b36084, &pc);
					_DBGPRT("CPU3    PC: 0x%08x\n", pc);
				}
			}
#endif
		}
	}
	return OK;
}

int32_t ADIv5::AP::select(uint32_t ap, uint32_t reg)
{
	uint32_t bank = reg & 0xF0;

	if (ap != lastAp || bank != lastApBank)
	{
		/* APSEL, APBANKSEL を設定 */
		DP_SELECT select;
		select.APSEL = ap;
		select.Reserved[0] = 0;
		select.Reserved[1] = 0;
		select.APBANKSEL = bank >> 4;
		select.DPBANKSEL = 0;

		int ret = dap.dpWrite(DP_REG_SELECT, select.raw);
		if (ret != OK) {
			return ret;
		}

		lastAp = ap;
		lastApBank = bank;
	}
	return OK;
}

errno_t ADIv5::clearError()
{
	DP_CTRL_STAT ctrlStat;
	errno_t ret = getCtrlStat(&ctrlStat);
	if (ret != OK)
		return ret;

	if (ctrlStat.STICKYERR || ctrlStat.WDATAERR)
	{
		if (ctrlStat.STICKYERR)
			_DBGPRT("Sticky Error is detected. ");
		if (ctrlStat.WDATAERR)
			_DBGPRT("Protocol Error is detected. ");
		_DBGPRT("Trying to clear... ");

		if (dap.getConnectionType() == DAP::JTAG)
		{
			ret = setCtrlStat(ctrlStat);
			if (ret != OK)
			{
				_DBGPRT("FAILED. (write)\n");
				return ret;
			}
		}
		else
		{
			DP_ABORT abort;
			abort.raw = 0;
			abort.STKERRCLR = 1;
			abort.WDERRCLR = 1;
			ret = dap.dpWrite(DP_REG_ABORT, abort.raw);
			if (ret != OK)
			{
				_DBGPRT("FAILED. (write)\n");
				return ret;
			}
		}

		ret = getCtrlStat(&ctrlStat);
		if (ret != OK)
		{
			_DBGPRT("FAILED. (read)\n");
			return ret;
		}

		if (ctrlStat.STICKYERR || ctrlStat.WDATAERR)
			_DBGPRT("FAILED. (clear)\n");
		else
			_DBGPRT("OK.\n");
	}
	return OK;
}

errno_t ADIv5::AP::checkStatus(uint32_t ap)
{
	// clear error and retry
	errno_t ret = adi.clearError();
	if (ret != OK)
		return ret;

	AP_IDR idr;
	ret = read(ap, AP_REG_IDR, &idr.raw);
	if (ret != OK) {
		return ret;
	}

	if (idr.raw == 0)
		return EINVAL;

	if (idr.isAHB())
	{
		AHB_AP_CSW csw;
		ret = read(ap, MEM_AP_REG_CSW, &csw.raw);
		if (ret != OK)
			return ret;

		if (!csw.MasterType || !csw.Hprot1 || !csw.DeviceEn)
		{
			csw.MasterType = 1;
			csw.Hprot1 = 1;
			csw.DeviceEn = 1;

			ret = write(ap, MEM_AP_REG_CSW, csw.raw);
			if (ret != OK)
				return ret;
		}
	}
	return OK;
}

int32_t ADIv5::AP::read(uint32_t ap, uint32_t reg, uint32_t *data)
{
	int ret = select(ap, reg);
	if (ret != OK)
	{
		errno_t ret2 = checkStatus(ap);
		if (ret2 != OK)
			return ret;
		ret = select(ap, reg);
	}
	if (ret != OK)
		return ret;

	ret = dap.apRead(reg, data);
	if (ret != OK)
	{
		errno_t ret2 = checkStatus(ap);
		if (ret2 != OK)
			return ret;
		ret = dap.apRead(reg, data);
	}
	return ret;
}

int32_t ADIv5::AP::write(uint32_t ap, uint32_t reg, uint32_t data)
{
	int ret = select(ap, reg);
	if (ret != OK)
	{
		errno_t ret2 = checkStatus(ap);
		if (ret2 != OK)
			return ret;
		ret = select(ap, reg);
	}
	if (ret != OK)
		return ret;

	ret = dap.apWrite(reg, data);
	if (ret != OK)
	{
		errno_t ret2 = checkStatus(ap);
		if (ret2 != OK)
			return ret;
		ret = dap.apWrite(reg, data);
	}
	return ret;
}

bool ADIv5::MEM_AP::isSameTAR(uint32_t addr)
{
	if (lastTAR == addr)
		return true;
	return false;
}

bool ADIv5::MEM_AP::isSame32BitAlignedTAR(uint32_t addr, uint32_t* reg)
{
	if (reg == nullptr)
		return false;

	if ((lastTAR & 0xFFFFFFF0) == (addr & 0xFFFFFFF0))
	{
		if ((addr & 0xF) == 0x0)
			*reg = MEM_AP_REG_BD0;
		else if ((addr & 0xF) == 0x4)
			*reg = MEM_AP_REG_BD1;
		else if ((addr & 0xF) == 0x8)
			*reg = MEM_AP_REG_BD2;
		else if ((addr & 0xF) == 0xC)
			*reg = MEM_AP_REG_BD3;
		return true;
	}
	return false;
}

bool ADIv5::MEM_AP::is32BitAligned(uint32_t addr)
{
	if ((addr & 0x3) == 0)
		return true;
	return false;
}

bool ADIv5::MEM_AP::is16BitAligned(uint32_t addr)
{
	if ((addr & 0x1) == 0)
		return true;
	return false;
}

int32_t ADIv5::MEM_AP::read(uint32_t addr, uint32_t *data)
{
	uint32_t reg;
	errno_t ret = setAccessSize(SIZE_32BIT);
	if (ret != OK)
		return ret;

	if (is32BitAligned(lastTAR) && isSame32BitAlignedTAR(addr, &reg))
	{
		ret = ap.read(index, reg, data);
		if (ret != OK)
			return ret;
	}
	else
	{
		ret = ap.write(index, MEM_AP_REG_TAR, addr);
		if (ret != OK) {
			return ret;
		}
		lastTAR = addr;

		ret = ap.read(index, MEM_AP_REG_DRW, data);
		if (ret != OK) {
			return ret;
		}
	}
	return OK;
}

int32_t ADIv5::MEM_AP::write(uint32_t addr, uint32_t val)
{
	uint32_t reg;
	ASSERT_RELEASE(is32BitAligned(addr));

	errno_t ret = setAccessSize(SIZE_32BIT);
	if (ret != OK)
		return ret;

	if (is32BitAligned(lastTAR) && isSame32BitAlignedTAR(addr, &reg))
	{
		ret = ap.write(index, reg, val);
		if (ret != OK)
			return ret;
	}
	else
	{
		int ret = ap.write(index, MEM_AP_REG_TAR, addr);
		if (ret != OK) {
			return ret;
		}
		lastTAR = addr;

		ret = ap.write(index, MEM_AP_REG_DRW, val);
		if (ret != OK) {
			return ret;
		}
	}
	return OK;
}

int32_t ADIv5::MEM_AP::write(uint32_t addr, uint16_t val)
{
	ASSERT_RELEASE(is16BitAligned(addr));

	errno_t ret = setAccessSize(SIZE_16BIT);
	if (ret != OK)
		return ret;

	if (!isSameTAR(addr))
	{
		ret = ap.write(index, MEM_AP_REG_TAR, addr);
		if (ret != OK) {
			return ret;
		}
		lastTAR = addr;
	}

	ret = ap.write(index, MEM_AP_REG_DRW, (addr & 2) ? ((uint32_t)val) << 16 : val);
	if (ret != OK)
		return ret;

	return OK;
}

int32_t ADIv5::MEM_AP::write(uint32_t addr, uint8_t val)
{
	errno_t ret = setAccessSize(SIZE_8BIT);
	if (ret != OK)
		return ret;

	if (!isSameTAR(addr))
	{
		ret = ap.write(index, MEM_AP_REG_TAR, addr);
		if (ret != OK) {
			return ret;
		}
		lastTAR = addr;
	}

	ret = ap.write(index, MEM_AP_REG_DRW,
		(addr & 3) == 3 ? ((uint32_t)val) << 24 :
		(addr & 3) == 2 ? ((uint32_t)val) << 16 :
		(addr & 3) == 1 ? ((uint32_t)val) <<  8 : val );
	if (ret != OK)
		return ret;

	return OK;
}
errno_t ADIv5::MEM_AP::setAccessSize(ADIv5::MEM_AP::AccessSize size)
{
	if (size != lastAccessSize)
	{
		MEM_AP_CSW csw;
		errno_t ret = ap.read(index, MEM_AP_REG_CSW, &csw.raw);
		if (ret != OK)
			return ret;

		if (size != csw.Size)
		{
			csw.Size = size;
			ret = ap.write(index, MEM_AP_REG_CSW, csw.raw);
			if (ret != OK)
				return ret;
			lastAccessSize = csw.Size;
		}
	}
	return OK;
}

int32_t ADIv5::ROM_TABLE::read()
{
	int ret;
	uint32_t addr = component->base;

	if (!component->isRomTable())
		return OK;

	_DBGPRT("ROM_TABLE\n");
	_DBGPRT("  Base    : 0x%08x\n", addr);

	uint32_t data;
	ret = component->ap.read(addr + 0xFCC, &data);
	if (ret != OK)
		return ret;
	sysmem = data ? true : false;
	_DBGPRT("    MEMTYPE : %s\n", sysmem ? "SYSMEM is present" : "SYSMEM is NOT present");

	component->print();

	while (1)
	{
		Entry entry;
		ret = component->ap.read(addr, &entry.raw);
		if (ret != OK)
			return ret;

		if (entry.raw == 0x00)
			break;

		uint32_t entryAddr = component->base + entry.addr();
		_DBGPRT("  ENTRY          : 0x%08x (addr: 0x%08x)\n", entry.raw, entryAddr);
		if (entry.FORMAT)
		{
			_DBGPRT("    Present      : %s\n", entry.PRESENT ? "yes" : "no");
			if (entry.PWR_DOMAIN_ID_VAILD)
				_DBGPRT("    Power Domain ID : %x\n", entry.PWR_DOMAIN_ID);

			if (entry.present())
			{
				std::shared_ptr<Component> child = std::make_shared<Component>(Memory(component->ap, entryAddr));
				
				ret = child->readPidCid();
				if (ret != OK)
				{
					_DBGPRT("    Failed to read child component\n");
					continue;
				}

				if (child->isRomTable())
				{
					_DBGPRT("-> CHILD\n");
					children.push_back(std::make_pair(entry, ROM_TABLE(child)));
					children.back().second.read();
					_DBGPRT("<-\n");
				}
				else
				{
					entries.push_back(std::make_pair(entry, child));
					child->print();
				}
			}
		}
		else
		{
			_DBGPRT("    Invalid entry\n");
		}
		addr += 4;
	}

	return OK;
}

void ADIv5::ROM_TABLE::each(std::function<void(std::shared_ptr<Component>)> func)
{
	for (auto c : children)
	{
		c.second.each(func);
	}

	for (auto e : entries)
	{
		func(e.second);
	}
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::find(std::function<bool(Component&)> func)
{
	std::vector<std::shared_ptr<Component>> v;

	for (auto ap : memAps)
	{
		ap.second.each([&v, &func](std::shared_ptr<Component> component) {
			if (func(*component))
				v.push_back(component);
		});
	}
	return v;
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::findARMv7ARDIF()
{
	return find([](Component& component) {
		return component.isARMv7ARDIF() ? true : false;
	});
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::findARMv6MSCS()
{
	return find([](Component& component) {
		return component.isARMv6MSCS() ? true : false;
	});
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::findARMv6MDWT()
{
	return find([](Component& component) {
		return component.isARMv6MDWT() ? true : false;
	});
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::findARMv7MDWT()
{
	return find([](Component& component) {
		return component.isARMv7MDWT() ? true : false;
	});
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::findARMv6MBPU()
{
	return find([](Component& component) {
		return component.isARMv6MBPU() ? true : false;
	});
}

std::vector<std::shared_ptr<ADIv5::Component>> ADIv5::findARMv7MFPB()
{
	return find([](Component& component) {
		return component.isARMv7MFPB() ? true : false;
	});
}

std::vector<std::shared_ptr<ADIv5::MEM_AP>> ADIv5::findSysmem()
{
	std::vector<std::shared_ptr<MEM_AP>> v;

	for (auto ap : memAps)
	{
		if (ap.second.isSysmem())
			v.push_back(ap.first);
	}
	for (auto ap : ahbSysmemAps)
		v.push_back(ap);
	return v;
}
