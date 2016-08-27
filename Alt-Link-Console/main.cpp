
#include "stdafx.h"

#include <thread>
#include <chrono>

#include "Alt-Link.h"
#include "CMSIS-DAP.h"
#include "ADIv5.h"
#include "ADIv5TI.h"
#include "RspServer.h"

void dump(ADIv5TI& ti, uint64_t start, uint32_t len)
{
	std::vector<uint32_t> a;
	int32_t ret = ti.readMemory(start, len, &a);
	if (ret != OK)
	{
		_ERRPRT("Failed to read memory. (0x%08x)\n", ret);
		return;
	}
	if (a.size() > 0)
	{
		uint32_t index = 0;
		for (auto v : a)
		{
			if (index % 4 == 0)
				_ERRPRT("0x%08llx: ", start + index * 4);
			_ERRPRT("0x%08x ", v);
			if (index % 4 == 3)
				_ERRPRT("\n");
			index++;
		}
	}
	else
	{
		_ERRPRT("Failed to read memory. (zero size)\n");
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	AltLink altlink;

	if (altlink.enumerate() != OK)
		return OK;

	auto devices = altlink.getDevices();
	
	if (devices.size() <= 0) {
		_ERRPRT("No CMSIS-DAP devices.\n");
		return OK;
	}

	if (devices[0]->open() != OK)
		return OK;

#if 0
	CMSISDAP::ConnectionType type = CMSISDAP::JTAG;//CMSISDAP::SWJ_SWD;
#elif 0
	CMSISDAP::ConnectionType type = CMSISDAP::SWJ_JTAG;
#else
	CMSISDAP::ConnectionType type = CMSISDAP::SWJ_SWD;
#endif

	if (devices[0]->setConnectionType(type) != OK)
		return OK;

	if (devices[0]->scanAPs() != OK)
		return OK;

	auto ti = devices[0]->getTI();

	ti->testHaltAndRun();

	//dump(ti, 0xFFFF0000, 0x80);
	//dump(ti, 0xFFFF0000, 0x80);
	//dump(ti, 0x1fefe000, 0x100);
	//dump(ti, 0x3fefe000, 0x100);

	startRspServer(ti);

	while (1)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	return 0;
}

