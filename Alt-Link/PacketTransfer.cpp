
#include "stdafx.h"
#include "common.h"

#include <iomanip>
#include <sstream>
#include <regex>

#include "PacketTransfer.h"
#include "Converter.h"

class {
public:
	static uint8_t get(const std::string& data)
	{
		uint32_t sum = 0;
		for (uint32_t i = 0; i < data.length(); i++)
		{
			sum += data[i];
			sum = sum % 256;
		}
		return sum;
	}

	static bool compare(const std::string& data, const std::string& hexCheckSum)
	{
		ASSERT_RELEASE(data.length() > 0 && hexCheckSum.length() == 2);
		uint32_t checkSum = get(data);
		return (checkSum == std::stoi(hexCheckSum, nullptr, 16));
	}
} checkSum;

void PacketTransfer::push(std::string& data)
{
	buffer += data;

	std::regex rePacket("^\\$([^\\$\\#]+)#([0-9a-fA-F][0-9a-fA-F])");
	std::smatch match;

	while (1)
	{
		if (buffer[0] == '+')
		{
			buffer = buffer.substr(1);

			printf("received plus!\n");
		}
		else if (buffer[0] == '-')
		{
			buffer = buffer.substr(1);

			printf("request resend received!\n");
			requestResend();
		}
		else if (buffer[0] == 0x03)
		{
			buffer = buffer.substr(1);

			printf("interrupt received!\n");
			interruptReceived();
		}
		else if (std::regex_match(buffer, match, rePacket))
		{
			std::string payload = match[1];
			std::string hexCheckSum = match[2];

			buffer = buffer.substr(match.length(0));

			if (!checkSum.compare(payload, hexCheckSum))
			{
				printf("checkSum error!\n");
				errorPacketReceived();
			}
			else
			{
				printf("packet received! (%s)\n", payload.c_str());
				packetReceived(unescape(payload));
			}
		}
		else
		{
			break;
		}
	}
}

std::string PacketTransfer::makePacket(const std::string& payload)
{
	std::string escaped = escape(payload);
	return "$" + escaped + "#" + Converter::toHex(checkSum.get(escaped));
}

std::string PacketTransfer::escape(const std::string data)
{
	std::string escaped;

	for (char c : data)
	{
		if (c == '{' || c == '$' || c == '#')
		{
			escaped.append(1, c ^ 0x20);
		}
		else
		{
			escaped.append(1, c);
		}
	}
	return escaped;
}

std::string PacketTransfer::unescape(const std::string data)
{
	std::string unescaped;
	bool escaped = false;

	for (char c : data)
	{
		if (escaped)
		{
			unescaped.append(1, c ^ 0x20);
		}
		else
		{
			if (c == '}')
			{
				escaped = true;
			}
			else
			{
				unescaped.append(1, c);
			}
		}
	}
	return unescaped;
}
