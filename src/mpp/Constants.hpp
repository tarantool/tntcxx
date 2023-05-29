#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <iostream> // TODO - make output to iostream optional?
#include <variant>
#include <limits>

namespace mpp {

namespace compact {
enum Family : uint8_t {
	MP_NIL  /* = 0x00 */,
	MP_IGNR  /* = 0x01 */,
	MP_BOOL /* = 0x02 */,
	MP_UINT /* = 0x03 */,
	MP_INT  /* = 0x04 */,
	MP_FLT  /* = 0x05 */,
	MP_DBL  /* = 0x06 */,
	MP_STR  /* = 0x07 */,
	MP_BIN  /* = 0x08 */,
	MP_ARR  /* = 0x09 */,
	MP_MAP  /* = 0x0A */,
	MP_EXT  /* = 0x0B */,
	MP_END
};
} // namespace compact {

using FamilyUnder_t = uint32_t;
enum Family : FamilyUnder_t {
	MP_NIL  = 1u << compact::MP_NIL,
	MP_IGNR = 1u << compact::MP_IGNR,
	MP_BOOL = 1u << compact::MP_BOOL,
	MP_UINT = 1u << compact::MP_UINT,
	MP_INT  = 1u << compact::MP_INT,
	MP_FLT  = 1u << compact::MP_FLT,
	MP_DBL  = 1u << compact::MP_DBL,
	MP_STR  = 1u << compact::MP_STR,
	MP_BIN  = 1u << compact::MP_BIN,
	MP_ARR  = 1u << compact::MP_ARR,
	MP_MAP  = 1u << compact::MP_MAP,
	MP_EXT  = 1u << compact::MP_EXT,
	MP_AINT = (1u << compact::MP_UINT) | (1u << compact::MP_INT),
	MP_AFLT = (1u << compact::MP_FLT) | (1u << compact::MP_DBL),
	MP_ANUM = (1u << compact::MP_UINT) | (1u << compact::MP_INT) |
		  (1u << compact::MP_FLT) | (1u << compact::MP_DBL),
	MP_NONE = 0,
	MP_ANY  = std::numeric_limits<FamilyUnder_t>::max(),
};

enum ReadError_t {
	READ_ERROR_NEED_MORE,
	READ_ERROR_BAD_MSGPACK,
	READ_ERROR_WRONG_TYPE,
	READ_ERROR_MAX_DEPTH_REACHED,
	READ_ERROR_ABORTED_BY_USER,
	READ_ERROR_END
};

enum ReadResult_t : FamilyUnder_t {
	READ_SUCCESS = 0,
	READ_NEED_MORE = 1u << READ_ERROR_NEED_MORE,
	READ_BAD_MSGPACK = 1u << READ_ERROR_BAD_MSGPACK,
	READ_WRONG_TYPE = 1u << READ_ERROR_WRONG_TYPE,
	READ_MAX_DEPTH_REACHED = 1u << READ_ERROR_MAX_DEPTH_REACHED,
	READ_ABORTED_BY_USER = 1u << READ_ERROR_ABORTED_BY_USER,
	READ_RESULT_END
};

inline const char *FamilyName[] = {
	"MP_NIL",
	"MP_IGNR",
	"MP_BOOL",
	"MP_UINT",
	"MP_INT",
	"MP_FLT",
	"MP_DBL",
	"MP_STR",
	"MP_BIN",
	"MP_ARR",
	"MP_MAP",
	"MP_EXT",
	"MP_BAD",
	"MP_NONE"
};
static_assert(std::size(FamilyName) == compact::MP_END + 2, "Smth is forgotten");

inline const char *FamilyHumanName[] = {
	"nil",
	"ignored",
	"bool",
	"uint",
	"int",
	"float",
	"double",
	"str",
	"bin",
	"arr",
	"map",
	"ext",
	"bad",
	"none"
};
static_assert(std::size(FamilyHumanName) == compact::MP_END + 2, "Smth is forgotten");

inline const char *ReadErrorName[] = {
	"READ_ERROR_NEED_MORE",
	"READ_ERROR_BAD_MSGPACK",
	"READ_ERROR_WRONG_TYPE",
	"READ_ERROR_MAX_DEPTH_REACHED",
	"READ_ERROR_ABORTED_BY_USER",
	"READ_ERROR_UNKNOWN",
	"READ_SUCCESS",
};
static_assert(std::size(ReadErrorName) == READ_ERROR_END + 2, "Forgotten");

inline constexpr Family
operator|(Family a, Family b)
{
	return static_cast<Family>(static_cast<FamilyUnder_t>(a) |
				   static_cast<FamilyUnder_t>(b));
}

inline constexpr Family
operator&(Family a, Family b)
{
	return static_cast<Family>(static_cast<FamilyUnder_t>(a) &
				   static_cast<FamilyUnder_t>(b));
}

inline constexpr ReadResult_t
operator|(ReadResult_t a, ReadResult_t b)
{
	return static_cast<ReadResult_t>(static_cast<FamilyUnder_t>(a) |
					 static_cast<FamilyUnder_t>(b));
}

inline constexpr ReadResult_t
operator&(ReadResult_t a, ReadResult_t b)
{
	return static_cast<ReadResult_t>(static_cast<FamilyUnder_t>(a) &
					 static_cast<FamilyUnder_t>(b));
}

inline constexpr ReadResult_t
operator~(ReadResult_t a)
{
	return static_cast<ReadResult_t>(~static_cast<FamilyUnder_t>(a));
}

inline std::ostream&
operator<<(std::ostream& strm, compact::Family t)
{
	if (t >= compact::Family::MP_END)
		return strm << FamilyName[compact::Family::MP_END]
			    << "(" << static_cast<uint64_t>(t) << ")";
	return strm << FamilyName[t];
}

inline std::ostream&
operator<<(std::ostream& strm, Family t)
{
	if (t == MP_NONE)
		return strm << FamilyName[compact::Family::MP_END + 1];
	static_assert(sizeof(FamilyUnder_t) == sizeof(t), "Very wrong");
	FamilyUnder_t base = t;
	bool first = true;
	do {
		static_assert(sizeof(unsigned) == sizeof(t), "Wrong ctz");
		unsigned part = __builtin_ctz(base);
		base ^= 1u << part;
		if (first)
			first = false;
		else
			strm << "|";
		strm << static_cast<compact::Family>(part);
	} while (base != 0);
	return strm;
}

inline std::ostream&
operator<<(std::ostream& strm, ReadError_t t)
{
	if (t >= READ_ERROR_END)
		return strm << ReadErrorName[READ_ERROR_END]
			    << "(" << static_cast<uint64_t>(t) << ")";
	return strm << ReadErrorName[t];
}

inline std::ostream&
operator<<(std::ostream& strm, ReadResult_t t)
{
	if (t == READ_SUCCESS)
		return strm << ReadErrorName[READ_ERROR_END + 1];
	static_assert(sizeof(FamilyUnder_t) == sizeof(t), "Very wrong");
	FamilyUnder_t base = t;
	bool first = true;
	do {
		static_assert(sizeof(unsigned) == sizeof(t), "Wrong ctz");
		unsigned part = __builtin_ctz(base);
		base ^= 1u << part;
		if (first)
			first = false;
		else
			strm << "|";
		strm << static_cast<ReadError_t>(part);
	} while (base != 0);
	return strm;
}

struct StrValue { uint32_t offset; uint32_t size; };
struct BinValue { uint32_t offset; uint32_t size; };
struct ArrValue { uint32_t offset; uint32_t size; };
struct MapValue { uint32_t offset; uint32_t size; };
struct ExtValue { int8_t type; uint8_t offset; uint32_t size; };

// The order of types must be exactly the same as in compact::Family!
using Value_t = std::variant<
	std::nullptr_t, bool, uint64_t, int64_t, float, double,
	StrValue, BinValue, ArrValue, MapValue, ExtValue
>;

} // namespace mpp {
