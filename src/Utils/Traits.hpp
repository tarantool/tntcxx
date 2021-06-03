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
 * is_char_ptr_v
 * is_integral_constant_v
 * uni_integral_base_t
 * uni_value
 * is_uni_integral_v
 * is_uni_bool_v
 * is_uni_integer_v
 */

#include <cstddef>
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

/**
 * Check whether the type is (cv) pointer to (cv) char.
 */
template <class T>
constexpr bool is_char_ptr_v = std::is_pointer_v<T> &&
	std::is_same_v<char, std::remove_cv_t<std::remove_pointer_t<T>>>;

/**
 * Check whether the type is std::integral_constant.
 */
namespace details {
template <class T, class _ = void>
struct is_integral_constant_h : std::false_type {};

template <class T>
constexpr size_t constexpr_test_size(T t) { return t ? 1 : 2; }

template <class T>
struct is_integral_constant_h<T, std::void_t<
	typename T::value_type,
	decltype(T::value),
	std::enable_if_t<!std::is_member_pointer_v<decltype(&T::value)>, void>,
	char[T::value == static_cast<typename T::value_type>(0) ? 1 : 2],
	char[constexpr_test_size(T::value)]>>
: std::is_same<std::add_const_t<typename T::value_type>, decltype(T::value)> {};
} // namespace details {

template <class T>
constexpr bool is_integral_constant_v =
	details::is_integral_constant_h<std::remove_cv_t<T>>::value;

/**
 * Safe value_type extractor by std::integral_constant type.
 * It is std::integral_constant::value_type if it an integral_constant,
 * or given type itself otherwise.
 */
namespace details {
template <class T, class _ = void>
struct uni_integral_base_h { using type = T; };
template <class T>
struct uni_integral_base_h<T, std::enable_if_t<is_integral_constant_v<T>, void>>
{
	using type = std::add_const_t<typename T::value_type>;
};
} // namespace details {
template <class T>
using uni_integral_base_t = typename details::uni_integral_base_h<T>::type;

/**
 * Universal value extractor. Return static value member for
 * std::integral_constant, or the value itself otherwise.
 */
template <class T>
constexpr uni_integral_base_t<T> uni_value([[maybe_unused]] T value)
{
	if constexpr (is_integral_constant_v<T>)
		return T::value;
	else
		return value;
}

/**
 * Check whether the type is universal integral, that is either integral
 * or integral_constant with integral base.
 */
template <class T>
constexpr bool is_uni_integral_v = std::is_integral_v<uni_integral_base_t<T>>;

/**
 * Check whether the type is universal bool, that is either bool
 * or integral_constant with bool base.
 */
template <class T>
constexpr bool is_uni_bool_v = std::is_same_v<bool,
	std::remove_cv_t<uni_integral_base_t<T>>>;

/**
 * Check whether the type is universal integer, that is either integer
 * or integral_constant with integer base. See is_integer_v for
 * integer definition.
 */
template <class T>
constexpr bool is_uni_integer_v = is_integer_v<uni_integral_base_t<T>>;

} // namespace tnt {
