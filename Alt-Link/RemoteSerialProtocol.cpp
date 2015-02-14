
#include "stdafx.h"
#include "RemoteSerialProtocol.h"
#include "Converter.h"
#include "common.h"

#include <sstream>

void RemoteSerialProtocol::processQuery(const std::string& payload)
{
	if (payload.find("qSupported:") == 0)
	{
		std::string packet = makePacket("PacketSize=3fff;Qbtrace:off-;Qbtrace:bts-");
		sendPacket(packet);
	}
	else if (payload.find("qTStatus") == 0)
	{
		//std::string packet = makePacket("T0;tnotrun:0");
		//sendPacket(packet);
		sendNotSupported();
	}
	else if (payload.find("qOffsets") == 0)
	{
		// relocation information
		sendPacket(makePacket("Text=0;Data=0;Bss=0"));
	}
	else if (payload.find("qSymbol:") == 0)
	{
		sendOKorError(0x00);
	}
	else if (payload == "qC")
	{
		std::string packet = makePacket("QC-1");
		sendPacket(packet);
	}
	else if (payload.find("qAttached") == 0)
	{
		std::string packet = makePacket("1");
		sendPacket(packet);
	}
	else if (payload.find("qRcmd") == 0)
	{
		// TODO
		// remote command (CMSIS-DAP?)
		sendNotSupported();
	}
	else if (payload.find("qXfer") == 0)
	{
		// TODO
		// transfer special data to and from targets
		sendNotSupported();
	}
	else
	{
		sendNotSupported();
	}
}

void RemoteSerialProtocol::processBreakWatchPoint(const std::string& payload)
{
	if (payload[2] != ',')
	{
		sendOKorError(0x01);
		return;
	}

	uint64_t addr;
	auto delimiter1 = Converter::extract(payload, ',', 3, false, &addr);

	if (delimiter1 == payload.npos)
	{
		sendOKorError(0x01);
	}

	int32_t kind;
	auto delimiter2 = Converter::extract(payload, ';', delimiter1 + 1, true, &kind);

	switch (payload[1])
	{
	case '0':
	case '1':
	{
		CMSISDAP::BreakPointType type;

		if (payload[1] == '0')
		{
			type = CMSISDAP::MEMORY;
		}
		else
		{
			type = CMSISDAP::HARDWARE;
		}

		if (payload[0] == 'Z')
		{
			targetInterface.setBreakPoint(type, addr, kind);
		}
		else
		{
			targetInterface.unsetBreakPoint(type, addr, kind);
		}
		break;
	}
	case '2':
	case '3':
	case '4':
	{
		CMSISDAP::WatchPointType type;

		if (payload[1] == '2')
		{
			type = CMSISDAP::WRITE;
		}
		else if (payload[1] == '3')
		{
			type = CMSISDAP::READ;
		}
		else
		{
			type = CMSISDAP::ACCESS;
		}

		if (payload[0] == 'Z')
		{
			targetInterface.setWatchPoint(type, addr, kind);
		}
		else
		{
			targetInterface.setWatchPoint(type, addr, kind);
		}
		break;
	}
	default:
		sendNotSupported();
	}
}

void RemoteSerialProtocol::processWriteMemory(const std::string& payload, bool isBinary)
{
	uint64_t addr;
	uint32_t len;
	auto delimiter1 = Converter::extract(payload, 1, ',', false, &addr);
	if (delimiter1 != payload.npos)
	{
		auto delimiter2 = Converter::extract(payload, delimiter1 + 1, ':', false, &len);
		if (delimiter2 != payload.npos)
		{
			std::string data = payload.substr(delimiter2 + 1);
			std::vector<uint8_t> buffer;
			if (isBinary)
			{
				std::copy(data.begin(), data.end(), std::back_inserter(buffer));
			}
			else
			{
				buffer = Converter::toByteArray(data);
			}
			if (buffer.size() != len)
			{
				sendOKorError(0x01);
			}
			else
			{
				sendOKorError(targetInterface.writeMemory(addr, len, buffer));
			}
			return;
		}
	}
	sendOKorError(0x01);
	return;
}

void RemoteSerialProtocol::interruptReceived()
{
	//sendAck();

	uint8_t signal;
	int32_t result = targetInterface.interrupt(&signal);

	if (result == 0)
	{
		sendPacket("T050B:EC3D0040;0D:E03D0040;0F:D8070040;"/*"S" + Converter::toHex(signal)*/);
	}
}

void RemoteSerialProtocol::packetReceived(const std::string& payload)
{
	sendAck();

	switch (payload[0])
	{
	case 'q':
	{
		processQuery(payload);
		break;
	}
	case '?':
	{
		std::string packet = makePacket("S05");
		sendPacket(packet);
		break;
	}
	case 'c':
	{
		if (payload.length() > 1)
		{
			targetInterface.setCurrentPC(std::stoll(payload.substr(1), nullptr, 16));
		}
		targetInterface.resume();
		break;
	}
	case 's':
	{
		if (payload.length() > 1)
		{
			targetInterface.setCurrentPC(std::stoll(payload.substr(1), nullptr, 16));
		}
		uint8_t signal;
		uint8_t result = targetInterface.step(&signal);
		sendPacket("S" + Converter::toHex(signal));
		break;
	}
	case 'H':
	{
		threadId[payload[1]] = std::stoi(payload.substr(2), nullptr, 16);
		std::string packet = makePacket("OK");
		sendPacket(packet);
		break;
	}
	case 'g':	// read general registers
	{
		std::vector<uint32_t> array;
		if (targetInterface.readGenericRegisters(&array) == OK)
		{
			std::string packet = makePacket(Converter::toHex(array));
			sendPacket(packet);
		}
		else
		{
			sendOKorError(0x01);
		}
		break;
	}
	case 'G':	// write general registers
	{
		std::vector<uint32_t> array = Converter::toUInt32Array(payload.substr(1));
		if (targetInterface.writeGenericRegisters(array) == OK)
		{
			sendOKorError(0x00);
		}
		else
		{
			sendOKorError(0x01);
		}
		break;
	}
	case 'p':	// read specific register
	{
		uint32_t n;
		Converter::toInteger(payload.substr(1), &n);
		uint32_t value;
		if (targetInterface.readRegister(n, &value) == OK)
		{
			std::string packet = makePacket(Converter::toHex(value));
			sendPacket(packet);
		}
		else
		{
			sendOKorError(0x01);
		}
		break;
	}
	case 'P':	// write specific register
	{
		// TODO 複数レジスタを指定されるとおかしくなる
		uint32_t n;
		auto delimiter = Converter::extract(payload, 1, '=', false, &n);
		uint32_t value = std::stoi(payload.substr(delimiter + 1), nullptr, 16);

		if (targetInterface.writeRegister(n, value) == OK)
		{
			sendOKorError(0x00);
		}
		else
		{
			sendOKorError(0x01);
		}
		break;
	}
	case 'm':	// read memory
	{
		uint64_t addr;
		auto delimiter = Converter::extract(payload, 1, ',', false, &addr);
		uint32_t len = std::stoi(payload.substr(delimiter + 1), nullptr, 16);

		if (delimiter != payload.npos)
		{
			std::vector<uint8_t> array;
			targetInterface.readMemory(addr, len, &array);
			std::string packet = makePacket(Converter::toHex(array));
			sendPacket(packet);
			break;
		}
		sendOKorError(0x01);
		break;
	}
	case 'M':		// write memory
	{
		processWriteMemory(payload, false);
		break;
	}
	case 'X':		// write memory (binary)
	{
		processWriteMemory(payload, true);
		break;
	}
	case 'z':
	case 'Z':
	{
		processBreakWatchPoint(payload);
		break;
	}
	default:
		sendNotSupported();
	}
}

void RemoteSerialProtocol::requestResend()
{
	resend();
}

void RemoteSerialProtocol::errorPacketReceived()
{
	sendNack();
}

int32_t RemoteSerialProtocol::sendAck()
{
	return send("+");
}

int32_t RemoteSerialProtocol::sendNack()
{
	return send("-");
}

int32_t RemoteSerialProtocol::sendOKorError(uint8_t error)
{
	std::string packet;

	if (error == 0)
	{
		packet = makePacket("OK");
	}
	else
	{
		packet = makePacket("E" + Converter::toHex(error));
	}
	return sendPacket(packet);
}

int32_t RemoteSerialProtocol::sendNotSupported()
{
	std::string packet = makePacket("");
	return sendPacket(packet);
}

int32_t RemoteSerialProtocol::resend()
{
	return sendPacket(lastPacket);
}

int32_t RemoteSerialProtocol::sendPacket(const std::string packet)
{
	lastPacket = packet;
	return send(packet);
}
