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

enum Family : uint8_t {
	MP_NIL  /* = 0x00 */,
	MP_IGNR  /* = 0x01 */,
	MP_BOOL /* = 0x02 */,
	MP_INT  /* = 0x03 */,
	MP_FLT  /* = 0x04 */,
	MP_STR  /* = 0x05 */,
	MP_BIN  /* = 0x06 */,
	MP_ARR  /* = 0x07 */,
	MP_MAP  /* = 0x08 */,
	MP_EXT  /* = 0x09 */,
	MP_END
};

inline const char *FamilyName[] = {
	"MP_NIL",
	"MP_IGNR",
	"MP_BOOL",
	"MP_INT",
	"MP_FLT",
	"MP_STR",
	"MP_BIN",
	"MP_ARR",
	"MP_MAP",
	"MP_EXT",
	"MP_BAD",
	"MP_NONE"
};
static_assert(std::size(FamilyName) == MP_END + 2, "Smth is forgotten");

inline const char *FamilyHumanName[] = {
	"nil",
	"ignored",
	"bool",
	"int",
	"float",
	"str",
	"bin",
	"arr",
	"map",
	"ext",
	"bad",
	"none"
};
static_assert(std::size(FamilyHumanName) == MP_END + 2, "Smth is forgotten");

inline std::ostream&
operator<<(std::ostream& strm, Family t)
{
	if (t >= Family::MP_END)
		return strm << FamilyName[Family::MP_END]
			    << "(" << static_cast<uint64_t>(t) << ")";
	return strm << FamilyName[t];
}

template <Family ...FAMILY>
struct family_sequence {
	static constexpr std::size_t size() noexcept
	{
		return sizeof ...(FAMILY);
	}
};

template <Family NEW_FAMILY, Family... FAMILY>
static constexpr auto family_sequence_populate(struct family_sequence<FAMILY...>)
{
	return family_sequence<NEW_FAMILY, FAMILY...>{};
}

template <Family ...FAMILY_A, Family ...FAMILY_B>
inline constexpr auto
operator+(family_sequence<FAMILY_A...>, family_sequence<FAMILY_B...>)
{
	return family_sequence<FAMILY_A..., FAMILY_B...>{};
}

namespace details {

template <Family NEEDLE, Family HEAD, Family ...TAIL>
struct family_sequence_contains_impl_h {
	static constexpr bool value =
		family_sequence_contains_impl_h<NEEDLE, TAIL...>::value;
};

template <Family NEEDLE, Family LAST>
struct family_sequence_contains_impl_h<NEEDLE, LAST> {
	static constexpr bool value = false;
};

template <Family NEEDLE, Family ...TAIL>
struct family_sequence_contains_impl_h<NEEDLE, NEEDLE, TAIL...> {
	static constexpr bool value = true;
};

template <Family NEEDLE>
struct family_sequence_contains_impl_h<NEEDLE, NEEDLE> {
	static constexpr bool value = true;
};

template <Family NEEDLE, Family ...HAYSTACK>
struct family_sequence_contains_h {
	static constexpr bool value =
		family_sequence_contains_impl_h<NEEDLE, HAYSTACK...>::value;
};

template <Family NEEDLE>
struct family_sequence_contains_h<NEEDLE> {
	static constexpr bool value = false;
};

} // namespace details

template <Family NEEDLE, Family ...HAYSTACK>
static constexpr bool family_sequence_contains(family_sequence<HAYSTACK...>) {
	return details::family_sequence_contains_h<NEEDLE, HAYSTACK...>::value;
}

template <Family ...FAMILY>
std::ostream&
operator<<(std::ostream& strm, family_sequence<FAMILY...>)
{
	if (sizeof ...(FAMILY) == 0)
		return strm << FamilyName[Family::MP_END + 1];
	size_t count = 0;
	((strm << (count++ ? ", " : "") << FamilyName[FAMILY]), ...);
	return strm;
}

} // namespace mpp
