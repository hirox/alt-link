
#include "stdafx.h"

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

#include "Alt-Link.h"

extern AltLink altlink;

struct Response
{
	uint32_t ret;
	std::string message;

	template <class Archive>
	void save(Archive & archive) const
	{
		archive(CEREAL_NVP(ret));
		if (message != "")
			archive(CEREAL_NVP(message));
	}
};

template <class Data>
struct ResponseWithData : public Response
{
	Data& data;

	explicit ResponseWithData(Data& _data) : data(_data) {}

	template <class Archive>
	void save(Archive & archive) const
	{
		archive(CEREAL_NVP(ret));
		if (message != "")
			archive(CEREAL_NVP(message));
		archive(CEREAL_NVP(data));
	}
};

class Handler: public Poco::Net::HTTPRequestHandler
{
private:
	Poco::Net::HTTPServerResponse* _response;

	std::shared_ptr<cereal::JSONOutputArchive> sendResponse(errno_t ret, std::string message = "") {
		Response res;
		res.ret = ret;
		res.message = message;
		
		std::ostream& rs = _response->send();
		auto archive = std::make_shared<cereal::JSONOutputArchive>(rs);
		res.save(*archive);
		return archive;
	}

	template <class Data>
	void sendResponseWithData(errno_t ret, Data& data, std::string message = "") {
		ResponseWithData<Data> res(data);
		res.ret = ret;
		res.message = message;

		std::ostream& rs = _response->send();
		cereal::JSONOutputArchive archive(rs);
		res.save(archive);
	}

	std::string getCommand(std::string request) {
		std::string command;

		std::stringstream ss(request);
		cereal::JSONInputArchive archive(ss);
		archive(CEREAL_NVP(command));

		return command;
	}

	template <class Data>
	void get(std::string request, std::string name, Data* data) {
		std::stringstream ss(request);
		cereal::JSONInputArchive archive(ss);
		archive(::cereal::make_nvp(name.c_str(), *data));
	}

	std::shared_ptr<AltLink::Device> getDevice(std::string request) {
		uint32_t index = 0;
		get(request, "index", &index);

		auto devices = altlink.getDevices();
		if (devices.size() <= index) { throw std::exception("invalid index"); }
		
		return devices[index];
	}

public:
	void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) {
		_response = &response;

		response.setChunkedTransferEncoding(true);
		response.setContentType("application/json");

		if (request.getContentType() != "application/json")
		{
			sendResponse(EINVAL);
			return;
		}

		try {
			std::string requestString(std::istreambuf_iterator<char>(request.stream()), {});
			std::string command = getCommand(requestString);

			if (command == "enumerate")
			{
				sendResponse(altlink.enumerate());
			}
			else if (command == "devices")
			{
				auto archive = sendResponse(OK);

				archive->setNextName("data");
				archive->startNode();
				archive->makeArray();

				for (auto item : altlink.getDevices())
					(*archive)(item->getDeviceInfo());

				archive->finishNode();
			}
			else if (command == "open")
			{
				auto device = getDevice(requestString);

				sendResponse(device->open());
			}
			else if (command == "pin")
			{
				auto device = getDevice(requestString);
				auto dap = device->getDAP();

				CMSISDAP::PIN pin;
				auto ret = dap->getPinStatus(&pin);

				if (ret == OK)
					sendResponseWithData(OK, pin);
				else
					sendResponse(ret);
			}
			else if (command == "setConnectionType")
			{
				auto device = getDevice(requestString);

				CMSISDAP::ConnectionType type;
				get(requestString, "type", &type);

				sendResponse(device->setConnectionType(type));
			}
			else if (command == "scan")
			{
				auto device = getDevice(requestString);

				sendResponse(device->scan());
			}
			else if (command == "apTable")
			{
				auto device = getDevice(requestString);
				auto adi = device->getADI();;

				auto archive = sendResponse(OK);
				archive->setNextName("data");
				archive->startNode();
				adi->serializeApTable(*archive);
				archive->finishNode();
			}
			else if (command == "testHaltAndRun")
			{
				auto device = getDevice(requestString);

				sendResponse(device->getTI()->testHaltAndRun());
			}
			else
			{
				sendResponse(ENOENT);
			}
			_DBGPRT("command: %s\n", command.c_str());
		} catch (std::exception e) {
			_DBGPRT("%s\n", e.what());
			sendResponse(EINVAL);
		}
	}
};

class HandlerFactory : public Poco::Net::HTTPRequestHandlerFactory 
{ 
public: 
	HandlerFactory() {}

	Poco::Net::HTTPRequestHandler * createRequestHandler(const Poco::Net::HTTPServerRequest &request) {
		return new Handler();
	}
};

std::shared_ptr<Poco::Net::HTTPServer> server;

void startHttpServer() {
	static const uint16_t PORT = 8080;
	Poco::Net::ServerSocket socket(PORT);

	Poco::Net::HTTPServerParams *params = new Poco::Net::HTTPServerParams();
	params->setMaxQueued(100);
	params->setMaxThreads(16);

	server = std::make_shared<Poco::Net::HTTPServer>(new HandlerFactory(), socket, params);
	server->start();
}