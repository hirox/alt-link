// stdafx.h : 標準のシステム インクルード ファイルのインクルード ファイル、または
// 参照回数が多く、かつあまり変更されない、プロジェクト専用のインクルード ファイル
// を記述します。
//

#pragma once

#include "targetver.h"

#include <tchar.h>
#include <cstdio>
#include <cstdint>

#include "error.h"
#include "assert.h"

#define POCO_STATIC

#define CONFIRM_SIZE(typeA, typeB) sizeof(typeA) == sizeof(typeB), "sizeof(" #typeA ") should be same as sizeof(" #typeB ")"
#define CONFIRM_UINT32(type) CONFIRM_SIZE(type, uint32_t)

#define _DBGPRT printf
#define _ERRPRT printf
