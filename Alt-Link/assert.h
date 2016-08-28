
#pragma once

#include <cassert>

#if defined(_DEBUG)
#define ASSERT_RELEASE(x) assert(x)
#else
#define ASSERT_RELEASE(x) do { if(!(x)) { abort(); } } while(0)
#endif

#define CONFIRM_SIZE(typeA, typeB) sizeof(typeA) == sizeof(typeB), "sizeof(" #typeA ") should be same as sizeof(" #typeB ")"
#define CONFIRM_UINT32(type) CONFIRM_SIZE(type, uint32_t)
