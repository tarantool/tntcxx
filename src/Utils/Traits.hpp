#pragma once
/*
 * Copyright 2010-2021 Tarantool AUTHORS: please see AUTHORS file.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * Generic trait library for type analysis at compile time.
 * Short list (see full description below):
 *
 * base_enum_t (safe for non-enums)
 * is_integer_v
 * is_signed_integer_v
 * is_unsigned_integer_v
 * is_bounded_array_v (c array)
 */

#include <type_traits>

namespace tnt {

/**
 * Safe underlying_type extractor by enum type.
 * Unlike std::underlying_type_t which is (can be) undefined for non-enum
 * type, the checker below reveals underlying type for enums and leaves the
 * type for the rest types.
 */
namespace details {
template <class T, bool IS_ENUM> struct base_enum_h;
template <class T> struct base_enum_h<T, false> { using type = T; };
template <class T> struct base_enum_h<T, true> {
	using type = std::underlying_type_t<T>;
};
} // namespace details {

template <class T> using base_enum_t =
	typename details::base_enum_h<T, std::is_enum_v<T>>::type;

/**
 * Check that the type can represent integer numbers.
 * More formally, is_integer_v is true for enum types and integral types
 * (see std::is_integral for the list) except bool.
 */
template <class T>
constexpr bool is_integer_v = std::is_enum_v<T> ||
	(std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>);

/**
 * Trait whether is_integer_v (see above) which is signed.
 */
template <class T>
constexpr bool is_signed_integer_v =
	is_integer_v<T> && std::is_signed_v<base_enum_t<T>>;

/**
 * Trait whether is_integer_v (see above) which is unsigned.
 */
template <class T>
constexpr bool is_unsigned_integer_v =
	is_integer_v<T> && std::is_unsigned_v<base_enum_t<T>>;

/**
 * Check whether the type is C bounded array (with certain size), like int [10].
 * Identical to std::is_bounded_array_v from C++20.
 * Note that cv qualifiers for C array are passed to its elements.
 */
namespace details {
template <class T>
struct is_bounded_array_h : std::false_type {};
template <class T, std::size_t N>
struct is_bounded_array_h<T[N]> : std::true_type {};
} //namespace details {
template <class T>
constexpr bool is_bounded_array_v = details::is_bounded_array_h<T>::value;

} // namespace tnt {
