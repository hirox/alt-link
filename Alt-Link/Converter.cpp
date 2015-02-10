
#include "stdafx.h"

#include "Converter.h"

void Converter::put(std::stringstream& stream, uint32_t data)
{
	put(stream, (uint8_t)data);	data = data >> 8;
	put(stream, (uint8_t)data);	data = data >> 8;
	put(stream, (uint8_t)data);	data = data >> 8;
	put(stream, (uint8_t)data);
}

void Converter::put(std::stringstream& stream, uint8_t data)
{
	stream << std::setfill('0') << std::setw(2) << std::hex << (unsigned)data;
}

std::vector<uint8_t> Converter::toByteArray(const std::string hex)
{
	std::istringstream stream(hex);
	stream >> std::hex;

	std::vector<uint8_t> array;
	while (stream)
	{
		uint8_t data;
		stream >> data;
		array.push_back(data);
	}
	return array;
}

std::vector<uint32_t> Converter::toUInt32Array(const std::string hex)
{
	std::istringstream stream(hex);
	stream >> std::hex;

	std::vector<uint32_t> array;
	while (stream)
	{
		uint32_t data;
		stream >> data;
		array.push_back(data);
	}
	return array;
}

void Converter::toInteger(const std::string hex, int32_t* out)
{
	*out = std::stoi(hex, nullptr, 16);
}

void Converter::toInteger(const std::string hex, uint32_t* out)
{
	*out = (unsigned)std::stoi(hex, nullptr, 16);
}

void Converter::toInteger(const std::string hex, int64_t* out)
{
	*out = (unsigned)std::stoll(hex, nullptr, 16);
}

void Converter::toInteger(const std::string hex, uint64_t* out)
{
	*out = std::stoull(hex, nullptr, 16);
}

std::string::size_type Converter::extract(const std::string& str, size_t offset, const char delimiter, bool extractEvenNoDelimiter, std::string* out)
{
	ASSERT_RELEASE(out != nullptr);

	auto delimiterPos = str.find(delimiter, offset);
	if (delimiterPos != str.npos)
	{
		*out = str.substr(offset, delimiterPos - offset);
	}
	else if (extractEvenNoDelimiter)
	{
		*out = str.substr(offset);
	}
	return delimiterPos;
}