
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "ADIv5.h"
#include "ARMv6MSCS.h"
#include "ARMv6MDWT.h"
#include "TargetInterface.h"

class ADIv5TI : public TargetInterface
{
private:
	ADIv5& adi;
	std::shared_ptr<ARMv6MSCS> scs;
	std::shared_ptr<ARMv6MDWT> dwt;
	std::shared_ptr<ADIv5::MEM_AP> mem;

public:
	ADIv5TI(ADIv5 _adi);

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
};