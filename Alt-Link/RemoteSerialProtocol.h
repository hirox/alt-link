
#pragma once

#include "PacketTransfer.h"
#include "CMSIS-DAP.h"

#include <map>

class RemoteSerialProtocol : public PacketTransfer
{
public:
	explicit RemoteSerialProtocol(TargetInterface& interface) : targetInterface(interface) {}

private:
	virtual void requestResend();
	virtual void errorPacketReceived();
	virtual void interruptReceived();
	virtual void packetReceived(const std::string& payload);

	void processQuery(const std::string& payload);
	void processBreakWatchPoint(const std::string& payload);
	void processBinaryTransfer(const std::string& payload);

	int32_t sendAck();
	int32_t sendNack();
	int32_t sendOKorError(uint8_t error);
	int32_t sendNotSupported();
	int32_t resend();
	int32_t sendPacket(const std::string packet);

	std::string lastPacket;
	TargetInterface& targetInterface;
	std::map<char, int32_t> threadId;

protected:
	virtual int32_t send(const std::string data) = 0;
};
