#pragma once

#include "stdafx.h"

class DAP
{
public:
	virtual int32_t dpRead(uint32_t reg, uint32_t *data)	= 0;
	virtual int32_t dpWrite(uint32_t reg, uint32_t val)		= 0;
	virtual int32_t apRead(uint32_t reg, uint32_t *data)	= 0;
	virtual int32_t apWrite(uint32_t reg, uint32_t val)		= 0;

	enum ConnectionType
	{
		JTAG,
		SWJ_JTAG,
		SWJ_SWD
	};
	virtual int32_t setConnectionType(ConnectionType type) = 0;
	ConnectionType getConnectionType() { return connectionType; }

protected:
	ConnectionType connectionType;
};
