
#include "stdafx.h"
#include "ADIv5TI.h"

#include <array>

enum Signal
{
	SIGINT		= 2,
	SIGTRAP		= 5
};

ADIv5TI::ADIv5TI(std::shared_ptr<ADIv5> _adi) : adi(_adi)
{
	auto _v7dif = adi->findARMv7ARDIF();
	if (_v7dif.size() > 0)
	{
		errno_t ret;

		_DBGPRT("ARMv7-A/R Debug Interface\n");
		for (auto _dif : _v7dif)
		{
			auto dif = std::make_shared<ARMv7ARDIF>(*_dif);
			v7dif.push_back(dif);

			dif->init(_dif->getPid().PART);

			for (uint32_t i = 0; i < 20; i++)
			{
				uint32_t pc;
				ret = dif->getPCSR(&pc);
				if (ret != OK)
				{
					_DBGPRT("  Failed to get PC(SR).\n");
				}
				else
				{
					uint32_t cid;
					ret = dif->getCIDSR(&cid);
					if (ret != OK)
						_DBGPRT("  PC(SR): 0x%08x, CID(SR): failed(0x%08x)\n", pc, ret);
					else
						_DBGPRT("  PC(SR): 0x%08x, CID(SR): 0x%08x\n", pc, cid);
				}
			}
			dif->printDSCR();
			dif->printPRSR();
		}
	}

	auto _scs = adi->findARMv6MSCS();
	if (_scs.size() > 0)
	{
		scs = std::make_shared<ARMv6MSCS>(*_scs[0]);

		_DBGPRT("ARMv6-M/v7-M SCS\n");
		ARMv6MSCS::CPUID cpuid;
		if (scs->readCPUID(&cpuid) == OK)
			cpuid.print();
		ARMv6MSCS::DEMCR demcr;
		if (scs->readDEMCR(&demcr) == OK)
			demcr.print();
		ARMv6MSCS::DFSR dfsr;
		if (scs->readDFSR(&dfsr) == OK)
			dfsr.print();
		scs->printDHCSR();
	}

	auto _dwt = adi->findARMv6MDWT();
	if (_dwt.size() > 0)
	{
		dwt = std::make_shared<ARMv6MDWT>(*_dwt[0]);
		_DBGPRT("ARMv6-M DWT\n");
		dwt->printPC();
		dwt->printCtrl();
	}

	auto _v7dwt = adi->findARMv7MDWT();
	if (_v7dwt.size() > 0)
	{
		dwt = std::make_shared<ARMv7MDWT>(*_v7dwt[0]);
		_DBGPRT("ARMv7-M DWT\n");
		dwt->printPC();
		dwt->printCtrl();
	}

	auto _bpu = adi->findARMv6MBPU();
	if (_bpu.size() > 0)
	{
		bpu = std::make_shared<ARMv6MBPU>(*_bpu[0]);
		_DBGPRT("ARMv6-M BPU\n");
		bpu->init();
		bpu->printCtrl();
	}

	auto _fpb = adi->findARMv7MFPB();
	if (_fpb.size() > 0)
	{
		fpb = std::make_shared<ARMv7MFPB>(*_fpb[0]);
		_DBGPRT("ARMv7-M FPB\n");
		fpb->init();
		fpb->printCtrl();
		fpb->printRemap();
	}

	auto _mem = adi->findSysmem();
	if (_mem.size() > 0)
	{
		mem = _mem[0];
	}
}

errno_t ADIv5TI::testHaltAndRun()
{
	errno_t ret = OK;

	if (v7dif.size() > 0)
	{
		uint32_t index = 0;
		std::vector<bool> halted;
		for (auto dif : v7dif)
		{
			_DBGPRT("  Halting CPU %d\n", index);
			ret = dif->halt();
			if (ret != OK)
			{
				_DBGPRT("    Failed! (0x%08x)\n", ret);
				halted[index] = false;
			}
			else
			{
				halted[index] = true;
			}
			index++;
		}

		index = 0;
		for (auto dif : v7dif)
		{
			if (halted[index] == false)
				continue;

			_DBGPRT("  CPU %d\n", index);
			dif->printDSCR();
			for (uint32_t reg = 0; reg < 15; reg++)
			{
				uint32_t value;
				ret = dif->readReg(reg, &value);
				if (ret != OK)
					_DBGPRT("    Failed to read register. (REG: %x, ret: 0x%08x)\n", reg, ret);
				else
					_DBGPRT("    R%-2d: 0x%08x\n", reg, value);
			}

			uint32_t pc;
			ret = dif->getPC(&pc);
			if (ret != OK)
				_DBGPRT("    Failed to get PC. (ret: 0x%08x)\n", ret);
			else
				_DBGPRT("    PC : 0x%08x\n", pc);

			index++;
		}

		index = 0;
		for (auto dif : v7dif)
		{
			if (halted[index] == false)
				continue;

			_DBGPRT("  Restarting CPU %d\n", index);
			ret = dif->run();
			if (ret != OK)
				_DBGPRT("    Failed! (0x%08x)\n", ret);
			index++;
		}
	}

	if (scs != nullptr)
	{
		// Debug state でないとレジスタ値は読めない
		ret = scs->halt();
		if (ret == OK)
			scs->printRegs();
		ret = scs->run();
	}

	return ret;
}

int32_t ADIv5TI::attach()
{
	if (scs)
		return scs->halt();

	return ENODEV;
}

void ADIv5TI::detach()
{
	if (scs)
		scs->run();
}

void ADIv5TI::setTargetThreadId()
{
	// TODO
}

void ADIv5TI::setCurrentPC(const uint64_t addr)
{
	// TODO
	(void)addr;
}

void ADIv5TI::resume()
{
	// continue command
	if (scs)
		scs->run();

	// [TODO] return error code
}

int32_t ADIv5TI::step(uint8_t* signal)
{
	ASSERT_RELEASE(signal != nullptr);

	*signal = 0x05;	// SIGTRAP

	if (scs)
		return scs->step();

	return ENODEV;
}

int32_t ADIv5TI::interrupt(uint8_t* signal)
{
	ASSERT_RELEASE(signal != nullptr);

	*signal = 0x05;	// SIGTRAP

	if (scs)
		return scs->halt();

	return ENODEV;
}

errno_t ADIv5TI::isRunning(bool* running, uint8_t* signal)
{
	ASSERT_RELEASE(running != nullptr);
	ASSERT_RELEASE(signal != nullptr);

	if (!scs)
		return ENODEV;

	bool halt;
	errno_t ret = scs->isHalt(&halt);
	if (ret != OK)
		return ret;

	if (!halt)
	{
		*running = true;
		*signal = 0;
		return OK;
	}

	ARMv6MSCS::DFSR dfsr;
	ret = scs->readDFSR(&dfsr);
	if (ret != OK)
		return ret;

	if (dfsr.raw == 0)
	{
		*running = true;
		*signal = 0;
	}
	else
	{
		*running = false;

		if (dfsr.EXTERNAL)
			*signal = SIGINT;
		else if (dfsr.VCATCH)
			*signal = SIGINT;
		else if (dfsr.DWTTRAP)
			*signal = SIGTRAP;
		else if (dfsr.BKPT)
			*signal = SIGTRAP;
		else if (dfsr.HALTED)
			*signal = SIGTRAP;

		_DBGPRT("found stop\n");
		dfsr.print();
	}
	return OK;
}

errno_t ADIv5TI::setBreakPoint(BreakPointType type, uint64_t addr, BreakPointKind kind)
{
	if (type == BreakPointType::HARDWARE)
	{
		if (bpu)
			return bpu->addBreakPoint((uint32_t)addr);
		else if (fpb)
			return fpb->addBreakPoint((uint32_t)addr);
	}
	return ERSP_NOT_SUPPORTED;
}

int32_t ADIv5TI::unsetBreakPoint(BreakPointType type, uint64_t addr, BreakPointKind kind)
{
	if (type == BreakPointType::HARDWARE)
	{
		if (bpu)
			return bpu->delBreakPoint((uint32_t)addr);
		else if (fpb)
			return fpb->delBreakPoint((uint32_t)addr);
	}
	return ERSP_NOT_SUPPORTED;
}

int32_t ADIv5TI::setWatchPoint(WatchPointType type, uint64_t addr, uint32_t kind)
{
	return ERSP_NOT_SUPPORTED;	// not supported
}

int32_t ADIv5TI::unsetWatchPoint(WatchPointType type, uint64_t addr, uint32_t kind)
{
	return ERSP_NOT_SUPPORTED;	// not supported
}

errno_t ADIv5TI::readRegister(const uint32_t n, uint32_t* out)
{
	ASSERT_RELEASE(out != nullptr);

	if (!scs)
		return ENODEV;

	ARMv6MSCS::REGSEL regsel = (ARMv6MSCS::REGSEL)n;
	int32_t ret;

	if (n == 19 || n == 20 || n == 21 || n == 22)
	{
		ret = scs->readReg(ARMv6MSCS::REGSEL::CONTROL_PRIMASK, out);
		if (ret == OK)
		{
			if (n == 19)		// PRIMASK
				*out = *out & 0xFF;
			else if (n == 20)	// BASEPRI
				*out = (*out >> 8) & 0xFF;
			else if (n == 21)	// FAULTMASK
				*out = (*out >> 16) & 0xFF;
			else if (n == 22)	// CONTROL
				*out = (*out >> 24) & 0xFF;
		}
		return ret;
	}
	return scs->readReg(regsel, out);
}

errno_t ADIv5TI::readRegister(const uint32_t n, uint64_t* out)
{
	ASSERT_RELEASE(out != nullptr);
	uint32_t value;
	int32_t result = readRegister(n, &value);
	if (result == OK)
	{
		*out = value;
	}
	return result;
}

errno_t ADIv5TI::readRegister(const uint32_t n, uint64_t* out1, uint64_t* out2)
{
	ASSERT_RELEASE(out1 != nullptr && out2 != nullptr);
	uint32_t value;
	int32_t result = readRegister(n, &value);
	if (result == OK)
	{
		*out1 = value;
		*out2 = 0;
	}
	return result;
}

errno_t ADIv5TI::writeRegister(const uint32_t n, const uint32_t data)
{
	if (!scs)
		return ENODEV;

	ARMv6MSCS::REGSEL regsel = (ARMv6MSCS::REGSEL)n;
	int32_t ret;

	if (n == 19 || n == 20 || n == 21 || n == 22)
	{
		uint32_t tmp;
		ret = scs->readReg(ARMv6MSCS::REGSEL::CONTROL_PRIMASK, &tmp);
		if (ret == OK)
		{
			if (n == 19)		// PRIMASK
				tmp = (tmp & 0xFFFFFF00) | (data & 0xFF);
			else if (n == 20)	// BASEPRI
				tmp = (tmp & 0xFFFF00FF) | ((data & 0xFF) << 8);
			else if (n == 21)	// FAULTMASK
				tmp = (tmp & 0xFF00FFFF) | ((data & 0xFF) << 16);
			else if (n == 22)	// CONTROL
				tmp = (tmp & 0x00FFFFFF) | ((data & 0xFF) << 24);
			ret = scs->writeReg(ARMv6MSCS::REGSEL::CONTROL_PRIMASK, tmp);
		}
		return ret;
	}
	return scs->writeReg(regsel, data);
}

errno_t ADIv5TI::writeRegister(const uint32_t n, const uint64_t data)
{
	// [TODO] support 64bit
	return writeRegister(n, data);
}

errno_t ADIv5TI::writeRegister(const uint32_t n, const uint64_t data1, const uint64_t data2)
{
	// [TODO] support 128bit
	return writeRegister(n, data1);
}

errno_t ADIv5TI::readGenericRegisters(std::vector<uint32_t>* array)
{
	ASSERT_RELEASE(array != nullptr);

	for (int i = 0; i < 16; i++)
	{
		uint32_t value;
		errno_t ret = readRegister(i, &value);
		if (ret != OK)
			return ret;
		array->push_back(value);
	}
	return OK;
}

errno_t ADIv5TI::writeGenericRegisters(const std::vector<uint32_t>& array)
{
	if (array.size() != 16)
		return EINVAL;

	for (int i = 0; i < 16; i++)
	{
		errno_t ret = writeRegister(i, array[i]);
		if (ret != OK)
			return ret;
	}
	return OK;
}

errno_t ADIv5TI::readMemory(uint64_t addr, uint32_t len, std::vector<uint8_t>* array)
{
	ASSERT_RELEASE(array != nullptr);

	if (!mem)
		return ENODEV;

	int32_t ret;
	uint32_t i = 0;
	for (; i < len / 4; i++)
	{
		uint32_t data;
		ret = mem->read((uint32_t)addr, &data);
		if (ret != OK)
			return ret;
		//_DBGPRT("readMemory 0x%08x 0x%08x\n", (uint32_t)addr, data);
		array->push_back(data & 0xFF);
		array->push_back((data >> 8) & 0xFF);
		array->push_back((data >> 16) & 0xFF);
		array->push_back((data >> 24) & 0xFF);
		addr += 4;
	}

	len -= i * 4;
	if (len > 0)
	{
		uint32_t data;
		ret = mem->read((uint32_t)addr, &data);
		if (ret != OK)
			return ret;
		if (len > 0) { array->push_back((data >> 0) & 0xFF); len--; }
		if (len > 0) { array->push_back((data >> 8) & 0xFF); len--; }
		if (len > 0) { array->push_back((data >> 16) & 0xFF); len--; }
		ASSERT_RELEASE(len == 0);
	}
	return OK;
}

errno_t ADIv5TI::readMemory(uint64_t addr, uint32_t len, std::vector<uint32_t>* array)
{
	ASSERT_RELEASE(array != nullptr);

	if (!mem)
		return ENODEV;

	if ((addr & 0x3) != 0 || (len % 4) != 0)
		return EINVAL;

	int32_t ret;
	uint32_t i = 0;
	for (; i < len / 4; i++)
	{
		uint32_t data;
		ret = mem->read((uint32_t)addr, &data);
		if (ret != OK)
			return ret;
		array->push_back(data);
		addr += 4;
	}
	return OK;
}

errno_t ADIv5TI::writeMemory(uint64_t addr, uint32_t len, const std::vector<uint8_t>& array)
{
	if (!mem)
		return ENODEV;

	errno_t ret;
	uint32_t i = 0;
	for (; i < len / 4; i++)
	{
		uint32_t data = (array[i * 4 + 3] << 24) | (array[i * 4 + 2] << 16) | (array[i * 4 + 1] << 8) | array[i * 4];
		ret = mem->write((uint32_t)addr, data);
		if (ret != OK)
			return ret;
		addr += 4;
	}

	len -= i * 4;
	if (len > 0)
	{
#if 0
		_DBGPRT("[WARNING] Unalighned write may cause unexpected result. 0x%08x 0x%08x\n", (uint32_t)addr, len);

		uint32_t data;
		ret = mem->read((uint32_t)addr, &data);
		if (ret != OK)
			return ret;
		if (len > 0) { data = (data & 0xFFFFFF00) | array[i * 4]; len--; }
		if (len > 0) { data = (data & 0xFFFF00FF) | (array[i * 4 + 1] << 8); len--; }
		if (len > 0) { data = (data & 0xFF00FFFF) | (array[i * 4 + 2] << 16); len--; }
		ASSERT_RELEASE(len == 0);

		ret = mem->write((uint32_t)addr, data);
		if (ret != OK)
			return ret;
#else
		uint32_t offset = 0;
		if (len >= 2)
		{
			ret = mem->write((uint32_t)addr, (uint16_t)((array[i * 4 + 1] << 8) | array[i * 4]));
			if (ret != OK)
				return ret;
			len -= 2;
			offset += 2;
			addr += 2;
		}
		if (len > 0)
		{
			ret = mem->write((uint32_t)addr, array[i * 4 + offset]);
			if (ret != OK)
				return ret;
			len--;
		}
		ASSERT_RELEASE(len == 0);
#endif
	}
	return OK;
}

errno_t ADIv5TI::monitor(const std::string command, std::string* output)
{
	ASSERT_RELEASE(output != nullptr);

	printf("monitor [%s]\n", command.c_str());

	// TODO
	(void)command;
	(void)output;
	return 0;
}

std::string ADIv5TI::targetXml(uint32_t offset, uint32_t length)
{
	std::string out = createTargetXml();
	return std::string(out, offset, out.size() <= offset + length ? out.size() - offset : length);
}

std::string ADIv5TI::createTargetXml()
{
	std::string out = R"(<?xml version="1.0"?><!DOCTYPE target SYSTEM "gdb-target.dtd">)";
	out.append(R"(<target version="1.0">)");
	{
		out.append(R"(<feature name="org.gnu.gdb.arm.m-profile">)");
		{
			out.append(R"(<reg name="r0" bitsize="32" regnum="0" type="int" group="general"/>)");
			out.append(R"(<reg name="r1" bitsize="32" regnum="1" type="int" group="general"/>)");
			out.append(R"(<reg name="r2" bitsize="32" regnum="2" type="int" group="general"/>)");
			out.append(R"(<reg name="r3" bitsize="32" regnum="3" type="int" group="general"/>)");

			out.append(R"(<reg name="r4" bitsize="32" regnum="4" type="int" group="general"/>)");
			out.append(R"(<reg name="r5" bitsize="32" regnum="5" type="int" group="general"/>)");
			out.append(R"(<reg name="r6" bitsize="32" regnum="6" type="int" group="general"/>)");
			out.append(R"(<reg name="r7" bitsize="32" regnum="7" type="int" group="general"/>)");
			out.append(R"(<reg name="r8" bitsize="32" regnum="8" type="int" group="general"/>)");
			out.append(R"(<reg name="r9" bitsize="32" regnum="9" type="int" group="general"/>)");
			out.append(R"(<reg name="r10" bitsize="32" regnum="10" type="int" group="general"/>)");
			out.append(R"(<reg name="r11" bitsize="32" regnum="11" type="int" group="general"/>)");
			out.append(R"(<reg name="r12" bitsize="32" regnum="12" type="int" group="general"/>)");
			out.append(R"(<reg name="sp" bitsize="32" regnum="13" type="data_ptr" group="general"/>)");
			out.append(R"(<reg name="lr" bitsize="32" regnum="14" type="int" group="general"/>)");
			out.append(R"(<reg name="pc" bitsize="32" regnum="15" type="code_ptr" group="general"/>)");
			out.append(R"(<reg name="xPSR" bitsize="32" regnum="16" type="int" group="general"/>)");
		}
		out.append(R"(</feature>)");

		out.append(R"(<feature name="org.gnu.gdb.arm.m-system">)");
		{
			out.append(R"(<reg name="msp" bitsize="32" regnum="17" type="data_ptr" group="system"/>)");
			out.append(R"(<reg name="psp" bitsize="32" regnum="18" type="data_ptr" group="system"/>)");
			out.append(R"(<reg name="primask" bitsize="1" regnum="19" type="int8" group="system"/>)");
			out.append(R"(<reg name="basepri" bitsize="8" regnum="20" type="int8" group="system"/>)");
			out.append(R"(<reg name="faultmask" bitsize="1" regnum="21" type="int8" group="system"/>)");
			out.append(R"(<reg name="control" bitsize="3" regnum="22" type="int8" group="system"/>)");
		}
		out.append(R"(</feature>)");
	}
	out.append(R"(</target>)");
	return out;
}
