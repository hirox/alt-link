
#pragma once

#ifdef _WIN32

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN // Windows ヘッダーから使用されていない部分を除外します。

#include <tchar.h>

#endif

#include <cstdio>
#include <cstdint>
#include <errno.h>

#include "error.h"
#include "assert.h"
#include "cereal.h"

#define POCO_STATIC

#define _DBGPRT printf
#define _ERRPRT printf

typedef int errno_t;
