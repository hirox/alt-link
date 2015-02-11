
#pragma once

#include <cassert>

#if defined(_DEBUG)
#define ASSERT_RELEASE(x) assert(x)
#else
#define ASSERT_RELEASE(x) do { if(!(x)) { abort(); } } while(0)
#endif
