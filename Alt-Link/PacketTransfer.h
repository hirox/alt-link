
#pragma once

#include <cstdint>
#include <string>

class PacketTransfer
{
public:
	void push(std::string& data);
	virtual ~PacketTransfer() {}

protected:
	class Packet
	{
	public:
		Packet() {}
		explicit Packet(std::string& d) : data(d) {}
		std::string toString() const { return data; }

	private:
		std::string data;
	};
	Packet makePacket(const std::string& payload);

	virtual void requestResend() = 0;
	virtual void errorPacketReceived() = 0;
	virtual void interruptReceived() = 0;
	virtual void packetReceived(const std::string& payload) = 0;

private:
	std::string escape(const std::string data);
	std::string unescape(const std::string data);

	std::string buffer;
};
