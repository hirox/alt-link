
#pragma once

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

namespace cereal
{
	// [E] overload function macros to support bit field serialization
	// [J] bit field は const ではない参照が取れないので cereal を使用して直接 serialize できない
	//     そのため uint32_t の変数を経由して serialize するようにしてかつ黒魔術で可変長引数に対応した
#define VA_HELPER_EXPAND(X) X
#define VA_HELPER_CAT(A, B) A ## B
#define VA_HELPER_SELECT(NAME, NUM) VA_HELPER_CAT(NAME ## _, NUM)
#define VA_HELPER_GET_COUNT(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15, N, ...) N 
#define VA_SIZE(...) VA_HELPER_EXPAND(VA_HELPER_GET_COUNT(__VA_ARGS__, 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0))
#define VA_SELECT(NAME, ...) VA_HELPER_SELECT( NAME, VA_SIZE(__VA_ARGS__) )(__VA_ARGS__)

#define BF_ARCHIVE(...) VA_SELECT(BF_ARCHIVE, __VA_ARGS__)
#define BF_ARCHIVE_1(_1) do { uint32_t x = _1; archive(::cereal::make_nvp(#_1, x)); _1 = static_cast<decltype(_1)>(x); } while(0)
#define BF_ARCHIVE_2(_1, _2) do { BF_ARCHIVE_1(_1); BF_ARCHIVE_1(_2); } while(0)
#define BF_ARCHIVE_3(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_2(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_4(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_3(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_5(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_4(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_6(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_5(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_7(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_6(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_8(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_7(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_9(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_8(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_10(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_9(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_11(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_10(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_12(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_11(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_13(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_12(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_14(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_13(__VA_ARGS__)); } while(0)
#define BF_ARCHIVE_15(_1, ...) do { BF_ARCHIVE_1(_1); VA_HELPER_EXPAND(BF_ARCHIVE_14(__VA_ARGS__)); } while(0)
}
