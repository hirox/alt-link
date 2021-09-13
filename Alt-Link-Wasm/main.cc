
#include "stdafx.h"

#include <emscripten.h>
#include <emscripten/bind.h>

#include <thread>
#include <chrono>

#include "Alt-Link.h"

EM_ASYNC_JS(int, js_hid_enumerate, (), {
	let devices = await navigator.hid.getDevices();
	devices.forEach(device => {
		console.log(`HID: ${device.productName}`);
	});
	if (window) window.hid_devices = devices;
	return devices.length;
});

EM_ASYNC_JS(bool, js_hid_open, (int index), {
	let devices = window ? window.hid_devices : undefined;
	if (!devices || devices.length <= index) return false;

    if (!devices[index].opened) {
        await devices[index].open();
		devices[index].addEventListener("inputreport", event => {
			window.hid_inputreport = { id: event.reportId, data: event.data };
			if (window.hid_inputreport_resolve) window.hid_inputreport_resolve();
		});
    }
	return devices[index].opened;
});

EM_ASYNC_JS(bool, js_hid_close, (int index), {
	let devices = window ? window.hid_devices : undefined;
	if (!devices || devices.length <= index) return false;

    if (devices[index].opened) {
        await devices[index].close();
    }
	return true;
});

EM_ASYNC_JS(int, js_hid_write, (int index, const uint8_t* data, size_t length), {
	let devices = window ? window.hid_devices : undefined;
	if (!devices || devices.length <= index) return false;

    if (devices[index].opened && length > 0) {
		const id = getValue(data, "i8");
		const array = Module.HEAPU8.slice(data + 1, data + 1 + length - 1);
        await devices[index].sendReport(id, array);
    }
	return 0;
});

EM_ASYNC_JS(int, js_hid_read, (int index, uint8_t* data, size_t length), {
	let devices = window ? window.hid_devices : undefined;
	if (!devices || devices.length <= index) return false;

    if (devices[index].opened && length > 0) {
		if (!window.hid_inputreport) {
			await new Promise((resolve) => {
				window.hid_inputreport_resolve = resolve;
			});
		}
		if (!window.hid_inputreport) {
			return -1;
		}
		const read_length = window.hid_inputreport.data.byteLength;
		if (length < read_length) {
			console.log(length);
			return -1;
		}
		//setValue(data, window.hid_inputreport.reportId, "i8");
		Module.HEAPU8.set(new Uint8Array(window.hid_inputreport.data.buffer), data);
		window.hid_inputreport_old = window.hid_inputreport;
		window.hid_inputreport = undefined;
		return read_length;
    }
	return -1;
});

class HIDApi : public HIDDevice {
public:
	virtual std::vector<Info> enumerate() override {
		std::vector<Info> ret = {};
		auto size = js_hid_enumerate();
		for (auto i = 0; i < size; i++) {
			ret.push_back({"none", "none", "none", L"none", L"none", static_cast<uint16_t>(i), 0});
		}
		return ret;
	};

	virtual bool open(const Info& info) override {
		if (js_hid_open(info.vid)) {
			index = info.vid;
			return true;
		}
		return false;
	};

	virtual void close() override {
		js_hid_close(index);
	};

	virtual int write(const uint8_t* data, size_t length) override {
		return js_hid_write(index, data, length);
	};

	virtual int read(uint8_t* data, size_t length) override {
		return js_hid_read(index, data, length);
	};

public:
	int index = 0;
} device;

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

AltLink altlink;

int main(int argc, char* argv[])
{
	if (altlink.enumerate(&device) != OK)
		return OK;

	auto devices = altlink.getDevices();
	
	if (devices.size() <= 0) {
		_ERRPRT("No CMSIS-DAP devices.\n");
		return OK;
	}

	if (devices[0]->open(&device) != OK)
		return OK;

#if 0
	{
		auto dap = devices[0]->getDAP();
		CMSISDAP::PIN pin;
		dap->getPinStatus(&pin);

		std::stringstream ss;
		{
			cereal::JSONOutputArchive archive(ss);
			archive(CEREAL_NVP(pin));
		}
		std::cout << ss.str() << std::endl;

		{
			cereal::JSONInputArchive iarchive(ss);
			iarchive(CEREAL_NVP(pin));
		}
		pin.print();
	}

	{
		auto dap = devices[0]->getDAP();
		auto dapInfo = dap->getDapInfo();

		std::stringstream ss;
		{
			cereal::JSONOutputArchive archive(ss);
			archive(CEREAL_NVP(dapInfo));
		}
		std::cout << ss.str() << std::endl;
	}
#endif

#if 0
	CMSISDAP::ConnectionType type = CMSISDAP::JTAG;//CMSISDAP::SWJ_SWD;
#elif 0
	CMSISDAP::ConnectionType type = CMSISDAP::SWJ_JTAG;
#else
	CMSISDAP::ConnectionType type = CMSISDAP::SWJ_SWD;
#endif

	if (devices[0]->setConnectionType(type) != OK)
		return OK;

	if (devices[0]->scan() != OK)
		return OK;

	auto ti = devices[0]->getTI();

	//dump(ti, 0xFFFF0000, 0x80);
	//dump(ti, 0xFFFF0000, 0x80);
	//dump(ti, 0x1fefe000, 0x100);
	//dump(ti, 0x3fefe000, 0x100);

	return 0;
}

