
#include "stdafx.h"
#include <stdint.h>

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>

#include "RemoteSerialProtocol.h"

class TCPConnection : public Poco::Net::TCPServerConnection
{
	class RSPtoDAP : public RemoteSerialProtocol
	{
	private:
		TCPConnection& connection;
		CMSISDAP dap;

		int32_t send(const std::string packet)
		{
			printf("send (%s)\n", packet.c_str());
			return connection.socket().sendBytes(packet.c_str(), packet.length());
		}

	public:
		RSPtoDAP(TCPConnection& outer)
			: RemoteSerialProtocol(dap), connection(outer) {}
	} rsp;

public:
	TCPConnection(const Poco::Net::StreamSocket &socket)
		: TCPServerConnection(socket), rsp(*this) {}

	void run(void)
	{
		const static uint32_t BUFFER_SIZE = 256;
		char buffer[BUFFER_SIZE + 1] = { 0 };
		uint32_t bytes = 1;

		while(bytes)
		{
			try
			{
				bytes = socket().receiveBytes(buffer, BUFFER_SIZE);
				if (bytes)
				{
					buffer[bytes] = 0;
					std::string str(buffer);
					rsp.push(str);
				}
			}
			catch (Poco::Exception&)
			{
			}
		}
	}
};

class ConnectionFactory : public Poco::Net::TCPServerConnectionFactory {
public:
	ConnectionFactory() {}
	virtual ~ConnectionFactory() {}

	virtual Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket &socket)
	{
		return new TCPConnection(socket);
	}
};

int _tmain(int argc, _TCHAR* argv[])
{
	static const uint16_t PORT = 1234;

	Poco::Net::ServerSocket socket(PORT);
	socket.listen();

	Poco::Net::TCPServer server(new ConnectionFactory(), socket);
	server.start();

	while (1)
	{
		Sleep(1000);
	}

	return 0;
}

