#pragma once

#include "stdafx.h"

class DAP
{
public:
	virtual int32_t dpRead(uint32_t reg, uint32_t *data)	= 0;
	virtual int32_t dpWrite(uint32_t reg, uint32_t val)		= 0;
	virtual int32_t apRead(uint32_t reg, uint32_t *data)	= 0;
	virtual int32_t apWrite(uint32_t reg, uint32_t val)		= 0;
};
