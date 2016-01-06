
#pragma once

#include "PacketTransfer.h"
#include "TargetInterface.h"

#include <map>

class RemoteSerialProtocol : public PacketTransfer
{
public:
	explicit RemoteSerialProtocol(TargetInterface& interface) : targetInterface(interface), attached(false) {}

private:
	virtual void requestResend();
	virtual void errorPacketReceived();
	virtual void interruptReceived();
	virtual void packetReceived(const std::string& payload);

	void processQuery(const std::string& payload);
	void processBreakWatchPoint(const std::string& payload);
	void processWriteMemory(const std::string& payload, bool isBinary = false);

	int32_t sendAck();
	int32_t sendNack();
	int32_t sendOK();
	int32_t sendError(uint8_t error = 0x01);
	int32_t sendOKorError(uint8_t error);
	int32_t sendNotSupported();
	int32_t resend();
	int32_t sendPacket(const PacketTransfer::Packet& packet);

	PacketTransfer::Packet lastPacket;
	TargetInterface& targetInterface;
	std::map<char, int32_t> threadId;
	bool attached;

protected:
	virtual int32_t send(const std::string& data) = 0;
};
