#pragma once

#include <cstdint>
#include <string>
#include <vector>

class HIDDevice
{
public:
	struct Info
	{
		std::string path;
		std::string productString;
		std::string serial;
		std::wstring wproductString;
		std::wstring wserial;
		uint16_t vid;
		uint16_t pid;

		template <class Archive>
		void serialize(Archive & ar)
		{
			ar(CEREAL_NVP(path), CEREAL_NVP(productString), CEREAL_NVP(serial), CEREAL_NVP(vid), CEREAL_NVP(pid));
		}
	};

    virtual ~HIDDevice() {};

    virtual std::vector<Info> enumerate() = 0;
    virtual bool open(const Info& info) = 0;
    virtual void close() = 0;
    virtual int write(const uint8_t* data, size_t length) = 0;
    virtual int read(uint8_t* data, size_t length) = 0;
};