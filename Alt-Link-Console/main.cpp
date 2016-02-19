
#include "stdafx.h"
#include "CMSIS-DAP.h"
#include "ADIv5.h"
#include "ADIv5TI.h"
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
	TCPConnection(const Poco::Net::StreamSocket &socket, TargetInterface& ti)
		: TCPServerConnection(socket), rsp(*this, ti) {}

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
	ConnectionFactory(TargetInterface& _ti) : ti(_ti) {}
	virtual ~ConnectionFactory() {}

	virtual Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket &socket)
	{
		return new TCPConnection(socket, ti);
	}

	TargetInterface& ti;
};

int _tmain(int argc, _TCHAR* argv[])
{
	CMSISDAP dap;
	int32_t ret = dap.initialize();
	if (ret != OK)
	{
		_ERRPRT("Failed to initialize CMSIS-DAP device. (0x%08x)\n", ret);
		return ret;
	}

#if 0
	CMSISDAP::ConnectionType type = CMSISDAP::JTAG;//CMSISDAP::SWJ_SWD;
#else
	CMSISDAP::ConnectionType type = CMSISDAP::SWJ_SWD;
#endif

	ret = dap.setConnectionType(type);
	if (ret != OK)
	{
		_ERRPRT("Failed to set connection type. (0x%08x)\n", ret);
		return ret;
	}

	ADIv5 adi(dap);

	if (type == CMSISDAP::JTAG || type == CMSISDAP::SWJ_JTAG)
	{
		std::vector<uint32_t> idcodes;
		ret = dap.getJtagIDCODEs(&idcodes);
		if (ret != OK)
		{
			_ERRPRT("Failed to get IDCODEs. (0x%08x)\n", ret);
			return ret;
		}
	}
	else if (type == CMSISDAP::SWJ_SWD)
	{
		// SWD 移行直後は Reset State なので IDCODE を読んで解除する
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
	}

#if 0
	ret = dap.resetLink();
	if (ret != OK) {
		(void)dap.cmdLed(CMSISDAP::RUNNING, false);
		return ret;
	}
#endif

	ret = adi.powerupDebug();
	if (ret != OK)
		return ret;

	ret = adi.scanAPs();
	if (ret != OK)
		return ret;

	ret = dap.cmdLed(CMSISDAP::RUNNING, false);
	if (ret != OK)
		return ret;

	ADIv5TI ti(adi);

	static const uint16_t PORT = 1234;
	Poco::Net::ServerSocket socket(PORT);
	socket.listen();

	Poco::Net::TCPServer server(new ConnectionFactory(ti), socket);
	server.start();

	while (1)
	{
		Sleep(1000);
	}

	return 0;
}

