
#include "stdafx.h"
#include <memory>

#include "RemoteSerialProtocol.h"

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>

class TCPConnection : public Poco::Net::TCPServerConnection
{
private:

	class RSPtoDAP : public RemoteSerialProtocol
	{
	private:
		TCPConnection& connection;

		int32_t send(const std::string& packet)
		{
			printf("send (%s)\n", packet.c_str());
			return connection.socket().sendBytes(packet.c_str(), packet.length());
		}

	public:
		RSPtoDAP(TCPConnection& outer, TargetInterface& ti)
			: RemoteSerialProtocol(ti), connection(outer) {}
	} rsp;

public:
	TCPConnection(const Poco::Net::StreamSocket &socket, std::shared_ptr<TargetInterface> ti)
		: TCPServerConnection(socket), rsp(*this, *ti) {}

	void run(void)
	{
		const static uint32_t BUFFER_SIZE = 256;
		char buffer[BUFFER_SIZE + 1] = { 0 };
		uint32_t bytes = 1;

		while(bytes)
		{
			try
			{
				if (!rsp.isEmpty() || socket().poll(Poco::Timespan(0, 1), Poco::Net::Socket::SELECT_READ))
				{
					bytes = socket().receiveBytes(buffer, BUFFER_SIZE);
					if (bytes)
					{
						std::string str(buffer, bytes);
						rsp.push(str);
					}
				}
				else
				{
					rsp.idle();
				}
			}
			catch (Poco::Exception&)
			{
				break;
			}
		}
	}
};

class ConnectionFactory : public Poco::Net::TCPServerConnectionFactory {
public:
	ConnectionFactory(std::shared_ptr<TargetInterface> _ti) : ti(_ti) {}
	virtual ~ConnectionFactory() {}

	virtual Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket &socket)
	{
		return new TCPConnection(socket, ti);
	}

	std::shared_ptr<TargetInterface> ti;
};

std::shared_ptr<Poco::Net::TCPServer> server;

void startRspServer(std::shared_ptr<TargetInterface> ti) {
	static const uint16_t PORT = 1234;
	Poco::Net::ServerSocket socket(PORT);
	socket.listen();

	server = std::make_shared<Poco::Net::TCPServer>(new ConnectionFactory(ti), socket);
	server->start();
}