
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#include "common.h"

class Converter
{
public:
	template <typename T>
	static std::string toHex(const T data)
	{
		std::stringstream stream;
		put(stream, data);
		return stream.str();
	}

	template <typename T>
	static std::string toHex(const std::vector<T> array)
	{
		std::stringstream stream;
		for (T data : array)
		{
			put(stream, data);
		}
		return stream.str();
	}

	static std::vector<uint8_t> toByteArray(const std::string hex);
	static std::vector<uint32_t> toUInt32Array(const std::string hex);
	
	static void toInteger(const std::string hex, int32_t* out);
	static void toInteger(const std::string hex, uint32_t* out);
	static void toInteger(const std::string hex, int64_t* out);
	static void toInteger(const std::string hex, uint64_t* out);

	template <typename T>
	static std::string::size_type extract(const std::string& str, size_t offset, const char delimiter, bool extractEvenNoDelimiter, T* out)
	{
		ASSERT_RELEASE(out != nullptr);

		std::string substr;
		auto delimiterPos = extract(str, offset, delimiter, extractEvenNoDelimiter, &substr);

		if (extractEvenNoDelimiter || delimiterPos != str.npos)
			toInteger(substr, out);

		return delimiterPos;
	}

private:
	static void put(std::stringstream& stream, uint32_t data);
	static void put(std::stringstream& stream, uint8_t data);
	static std::string::size_type extract(const std::string& str, size_t offset, const char delimiter, bool extractEvenNoDelimiter, std::string* out);
};