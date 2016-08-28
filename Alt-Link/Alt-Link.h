#pragma once

#include "CMSIS-DAP.h"
#include "ADIv5.h"
#include "ADIv5TI.h"

class AltLink {
public:
	class Device {
		CMSISDAP::DeviceInfo info;
		CMSISDAP::ConnectionType connectionType;
		bool opened;
		bool scanned;
		std::shared_ptr<CMSISDAP> dap;
		std::shared_ptr<ADIv5> adi;
		std::shared_ptr<ADIv5TI> ti;

		struct DeviceFlags
		{
			bool autoPowerUpDebugBlock;
			bool autoEnableDataWatchpointAndTraceBlock;

			DeviceFlags() :
				autoPowerUpDebugBlock(true), autoEnableDataWatchpointAndTraceBlock(true) {}
		} flags;

	private:
		errno_t scanAPs() {
			errno_t ret;

			if (opened == false)
				return EFAULT;

			ti = nullptr;
			adi = std::make_shared<ADIv5>(dap);

			if (connectionType == CMSISDAP::JTAG || connectionType == CMSISDAP::SWJ_JTAG)
			{
				ret = dap->scanJtagDevices();
				if (ret != OK)
					return ret;

#if 0
				ADIv5::DP_IDCODE idcode;
				ret = adi.getIDCODE(&idcode);
				if (ret != OK)
				{
					_ERRPRT("Could not read DP_IDCODE. (0x%08x)\n", ret);
					if (ret == CMSISDAP_ERR_NO_ACK)
						_ERRPRT("Target did not respond. Please check the electrical connection.\n");
					return ret;
		}
				idcode.print();
#endif
			}
			else if (connectionType == CMSISDAP::SWJ_SWD)
			{
				// SWD 移行直後は Reset State なので IDCODE を読んで解除する
				ADIv5::DP_IDCODE idcode;
				ret = adi->getIDCODE(&idcode);
				if (ret != OK)
				{
					_ERRPRT("Could not read DP_IDCODE. (0x%08x)\n", ret);
					if (ret == CMSISDAP_ERR_NO_ACK)
						_ERRPRT("Target did not respond. Please check the electrical connection.\n");
					return ret;
				}
				idcode.print();
			}

#if 0
			ret = dap.resetLink();
			if (ret != OK) {
				(void)dap.cmdLed(CMSISDAP::RUNNING, false);
				return ret;
			}
#endif

			if (flags.autoPowerUpDebugBlock)
			{
				ret = adi->powerupDebug();
				if (ret != OK)
					return ret;
			}

			ret = adi->scanAPs();
			if (ret != OK)
				return ret;

			scanned = true;
			return ret;
		}

	public:
		Device(CMSISDAP::DeviceInfo _info)
			: info(_info), opened(false), scanned(false), adi(nullptr), dap(nullptr), ti(nullptr),
			connectionType(CMSISDAP::SWJ_SWD) {}

		errno_t open() {
			dap = CMSISDAP::open(info);
			if (dap == nullptr)
			{
				_ERRPRT("Failed to open CMSIS-DAP device.\n");
				return EFAULT;
			}

			errno_t ret = dap->initialize();
			if (ret != OK)
			{
				_ERRPRT("Failed to initialize CMSIS-DAP device. (0x%08x)\n", ret);
				return ret;
			}

			opened = true;
			return ret;
		}

		errno_t setConnectionType(CMSISDAP::ConnectionType type) {
			if (opened == false)
				return EFAULT;

			connectionType = type;

			errno_t ret = dap->setConnectionType(type);
			if (ret != OK)
			{
				_ERRPRT("Failed to set connection type. (0x%08x)\n", ret);
				return ret;
			}

			scanned = false;
			return ret;
		}

		errno_t isDataWatchpointAndTraceBlockEnabled(bool* enabled) {
			errno_t ret;
			if (ti == nullptr || enabled == nullptr)
				return EFAULT;

			auto scs = ti->getARMv6MSCS();
			if (scs == nullptr)
				return EFAULT;

			ARMv6MSCS::DEMCR demcr;
			ret = scs->readDEMCR(&demcr);
			if (ret != OK)
				return ret;

			if (demcr.DWTENA)
				*enabled = true;
			else
				*enabled = false;

			return ret;
		}

		errno_t enableDataWatchpointAndTraceBlock() {
			errno_t ret;
			if (ti == nullptr)
				return EFAULT;

			auto scs = ti->getARMv6MSCS();
			if (scs == nullptr)
				return EFAULT;

			ARMv6MSCS::DEMCR demcr;
			ret = scs->readDEMCR(&demcr);
			if (ret != OK)
				return ret;

			if (!demcr.DWTENA)
			{
				demcr.DWTENA = 1;
				ret = scs->writeDEMCR(demcr);
				if (ret != OK)
					return ret;

				// rescan
				ret = scanAPs();
				if (ret != OK)
					return ret;
			}
			return OK;
		}

		errno_t scan() {
			if (scanned)
				return OK;

			auto ret = scanAPs();
			if (ret != OK)
				return ret;

			if (adi == nullptr)
				return EFAULT;

			if (ti == nullptr)
				ti = std::make_shared<ADIv5TI>(adi);

			if (!flags.autoEnableDataWatchpointAndTraceBlock)
				return OK;

			bool enabled;
			if (isDataWatchpointAndTraceBlockEnabled(&enabled) == OK)
			{
				if (!enabled)
				{
					_DBGPRT("Enabling Data Watchpoint and Trace Block.\n");
					if (enableDataWatchpointAndTraceBlock() == OK)
						ti = std::make_shared<ADIv5TI>(adi);
				}
			}

			return OK;
		}

		std::shared_ptr<ADIv5TI> getTI() {
			if (scanned == false)
				return nullptr;


			if (ti == nullptr && adi != nullptr)
				ti = std::make_shared<ADIv5TI>(adi);

			return ti;
		}

		std::shared_ptr<CMSISDAP> getDAP() { return dap; }
		std::shared_ptr<ADIv5> getADI() { return adi; }
		CMSISDAP::DeviceInfo& getDeviceInfo() { return info; }
	};

private:
	std::vector<std::shared_ptr<Device>> devices;

public:
	errno_t enumerate() {
		// close all opened devices
		devices.clear();

		std::vector<CMSISDAP::DeviceInfo> info;
		errno_t ret = CMSISDAP::enumerate(&info);
		if (ret != OK)
		{
			_ERRPRT("Failed to enumerate device. (0x%08x)\n", ret);
			return ret;
		}

		for (auto i : info)
			devices.push_back(std::make_shared<Device>(i));

		return ret;
	}

	std::vector<std::shared_ptr<Device>>& getDevices() { return devices; }
};