
#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。

#include <tchar.h>
#include <cstdio>
#include <cstdint>
#include <errno.h>

#include "error.h"
#include "assert.h"

#define POCO_STATIC

#define CONFIRM_SIZE(typeA, typeB) sizeof(typeA) == sizeof(typeB), "sizeof(" #typeA ") should be same as sizeof(" #typeB ")"
#define CONFIRM_UINT32(type) CONFIRM_SIZE(type, uint32_t)

#define _DBGPRT printf
#define _ERRPRT printf
