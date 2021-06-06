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
 * tuple_size_v (standard (tuple, array, pair) + bounded array)
 * tuple_element_t<I>
 * has_get_by_type_v<T, U>
 * has_get_by_size_v<I, U>
 * get<T>
 * get<I>
 * is_tuplish_v (standard (tuple, array, pair) + bounded array)
 * is_pairish_v
 * is_tuplish_of_pairish_v
 * is_variant_v
 * is_optional_v
 * is_member_ptr_v
 * is_uni_member_ptr_v
 * member_class_t
 * uni_member_class_t
 * demember_t
 * uni_demember_t
 * uni_member
 */

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <variant>

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

/**
 * Universal tuple_size.
 * If a type is a C bounded array - return its size.
 * If std::tuple_size is defined - use it.
 * Undefined otherwise.
 */
template <class T, class _ = void>
struct tuple_size;

template <class T>
struct tuple_size<T, std::enable_if_t<is_bounded_array_v<T>, void>> : std::extent<T> {};

template <class T>
struct tuple_size<T, std::void_t<char [sizeof(std::tuple_size<T>)]>> : std::tuple_size<T> {};

template <class T>
constexpr size_t tuple_size_v = tuple_size<T>::value;

/**
 * Universal tuple_element.
 * If a type is a C bounded array - return type of element.
 * If std::tuple_element is defined - use it.
 * Undefined otherwise.
 */
template <size_t I, class T, class _ = void>
struct tuple_element;

template <size_t I, class T>
struct tuple_element<I, T,
	std::enable_if_t<is_bounded_array_v<T>, void>>
: std::remove_extent<T> {};

template <size_t I, class T>
struct tuple_element<I, T,
	std::void_t<char [sizeof(std::tuple_element<I, std::remove_cv_t<T>>)]>>
: std::tuple_element<I, T> {};

template <size_t I, class T>
using tuple_element_t = typename tuple_element<I, T>::type;

/**
 * Checker that a type has get<T> and get <I> methods.
 */
namespace details {
template <class T, class U, class _ = void>
struct has_get_by_type_h : std::false_type {};
template <class T, class U>
struct has_get_by_type_h<T, U,
	std::void_t<decltype(std::declval<U>().template get<T>())>>
: std::true_type { };

template <size_t I, class U, class _ = void>
struct has_get_by_size_h : std::false_type {};
template <size_t I, class U>
struct has_get_by_size_h<I, U,
	std::void_t<decltype(std::declval<U>().template get<I>())>>
: std::true_type { };
} //namespace details {

template <class T, class U>
constexpr bool has_get_by_type_v = details::has_get_by_type_h<T, U>::value;
template <size_t I, class U>
constexpr bool has_get_by_size_v = details::has_get_by_size_h<I, U>::value;

/**
 * Universal get by type.
 * 1. Call `get` method template, if present.
 * 2. Call std::get otherwise.
 */
template <class T, class U>
constexpr std::enable_if_t<has_get_by_type_v<T, U>, T&> get(U& u)
{
	return u.template get<T>();
}
template <class T, class U>
constexpr std::enable_if_t<has_get_by_type_v<T, U>, const T&> get(const U& u)
{
	return u.template get<T>();
}

template <class T, class U>
constexpr std::enable_if_t<!has_get_by_type_v<T, U>, T&> get(U& u)
{
	return std::get<T>(u);
}
template <class T, class U>
constexpr std::enable_if_t<!has_get_by_type_v<T, U>, const T&> get(const U& u)
{
	return std::get<T>(u);
}

/**
 * Universal get by index.
 * 1. Call [] for C arrays.
 * 2. Call `get` method template, if present.
 * 3. Call std::get otherwise.
 */
template <size_t I, class U>
constexpr std::enable_if_t<is_bounded_array_v<U>,
	std::remove_extent_t<U>&> get(U& u)
{
	return u[I];
}

template <size_t I, class U>
constexpr std::enable_if_t<has_get_by_size_v<I, U>,
	decltype(std::declval<U&>().template get<I>())> get(U& u)
{
	return u.template get<I>();
}

template <size_t I, class U>
constexpr std::enable_if_t<has_get_by_size_v<I, U>,
	decltype(std::declval<const U&>().template get<I>())> get(const U& u)
{
	return u.template get<I>();
}

template <size_t I, class U>
constexpr std::enable_if_t<!is_bounded_array_v<U> && !has_get_by_size_v<I, U>,
	decltype(std::get<I>(std::declval<U&>()))> get(U& u)
{
	return std::get<I>(u);
}

template <size_t I, class U>
constexpr std::enable_if_t<!is_bounded_array_v<U> && !has_get_by_size_v<I, U>,
	decltype(std::get<I>(std::declval<const U&>()))> get(const U& u)
{
	return std::get<I>(u);
}

/**
 * Check whether the type is compatible with std::tuple, that is a fixed-size
 * collection of values. It can be an bounded array or anything that is
 * accessible with std::tuple_size and std::tuple_element. In other words it
 * is anything that is accessible with tnt::tuple_size and tnt::tuple_element.
 * From STL it can be std::tuple, std::pair, std::array and generic array.
 * It is expected that an instance of such a type is also accessible with
 * tnt::get, but unfortunately this checker doesn't check that.
 */
namespace details {

template <class T, class IDXS>
struct tuplish_types_h;

template <class T, size_t... I>
struct tuplish_types_h<T, std::index_sequence<I...>> {
	using type = std::tuple<tnt::tuple_element_t<I, T>...>;
};

template <class T, class _ = void>
struct is_tuplish_h : std::false_type {};

template <class T>
struct is_tuplish_h<T, std::void_t<
	char [sizeof(tnt::tuple_size<T>)],
	typename tuplish_types_h<T, std::make_index_sequence<tnt::tuple_size_v<T>>>::type>>
: std::true_type {};
} //namespace details {

template <class T>
constexpr bool is_tuplish_v = details::is_tuplish_h<std::remove_cv_t<T>>::value;

/**
 * Check whether the type is compatible with std::pair, that is a struct
 * with `first` and `second` members and has is accessible with tuple_size
 * and tuple_element.
 * It is expected that an instance of such a type is also accessible with
 * tnt::get, but unfortunately this checker doesn't check that.
 */
namespace details {
template <class T, class _ = void>
struct is_pairish_h : std::false_type {};

template <class T>
struct is_pairish_h<T, std::void_t<
	char [sizeof(tnt::tuple_size<T>)],
	std::enable_if_t<tnt::tuple_size_v<T> == 2, void>,
	typename tuplish_types_h<T, std::make_index_sequence<tnt::tuple_size_v<T>>>::type,
	decltype(std::declval<T&>().first),
	decltype(std::declval<T&>().second)>>
: std::true_type {};
} //namespace details {

template <class T>
constexpr bool is_pairish_v = details::is_pairish_h<std::remove_cv_t<T>>::value;

/**
 * Check whether the type is compatible with std::tuple (see is_tuplish_v) and
 * consist of types that are compatible with std::pair (see is_pairish_v).
 */
namespace details {
template<class T, bool IS_TUPLISH, class IS>
struct is_tuplish_of_pairish_h : std::false_type {};

template<class T, size_t ...I>
struct is_tuplish_of_pairish_h<T, true, std::index_sequence<I...>> {
	static constexpr bool value = (is_pairish_v<tuple_element_t<I, T>> && ...);
};

template<class T>
struct is_tuplish_of_pairish_h<T, true, void>
: is_tuplish_of_pairish_h<T, true, std::make_index_sequence<tuple_size_v<T>>> {};
} //namespace details {

template <class T>
constexpr bool is_tuplish_of_pairish_v =
	details::is_tuplish_of_pairish_h<std::remove_cv_t<T>, is_tuplish_v<T>, void>::value;

/**
 * Check whether the type is looks like std::variant. That means that it
 * is accessible with std variant_size, variant_alternative and get.
 */
namespace details {
template <class T, class _ = void>
struct is_variant_h : std::false_type {};
template <class T>
struct is_variant_h<T, std::void_t<
	decltype(std::declval<T>().index()),
	decltype(std::variant_size<T>::value),
	typename std::variant_alternative<0, T>::type,
	decltype(tnt::get<std::variant_alternative_t<0, T>>(std::declval<T>()))>>
: std::is_same<size_t, std::decay_t<decltype(std::declval<T>().index())>> {};
} //namespace details {

template <class T>
constexpr bool is_variant_v =
	!std::is_reference_v<std::remove_cv_t<T>> &&
	details::is_variant_h<std::remove_cv_t<T>>::value;

/**
 * Check whether the type looks like std::optional, at least it has
 * operator bool, operator *, bool has_value() and value() methods.
 */
namespace details {
template <class T, class _ = void>
struct is_optional_h : std::false_type {};
template <class T>
struct is_optional_h<T, std::void_t<
	decltype(std::declval<T>().has_value()),
	decltype((bool)std::declval<T>()),
	decltype(std::declval<T>().value()),
	decltype(*std::declval<T>())>>
: std::is_same<bool, std::decay_t<decltype(std::declval<T>().has_value())>> {};
} // namespace details {

template <class T>
constexpr bool is_optional_v =
	!std::is_reference_v<std::remove_cv_t<T>> &&
	details::is_optional_h<std::remove_cv_t<T>>::value;

/**
 * Check whether the type is pointer to member (member object, not method).
 */
template <class T>
constexpr bool is_member_ptr_v = std::is_member_object_pointer_v<T>;

/**
 * Check whether the type is pointer to member OR integral constant with it.
 */
template <class T>
constexpr bool is_uni_member_ptr_v =
	std::is_member_object_pointer_v<uni_integral_base_t<T>>;

/**
 * Safe getter that for pointer to member returns class and original
 * type in any other case.
 */
namespace details {
template <class T> struct member_class_h { using type = T; };
template <class T, class U> struct member_class_h<T U::*> { using type = U; };
} //namespace details {

template <class T>
using member_class_t = std::conditional_t<is_member_ptr_v<T>,
	typename details::member_class_h<std::remove_cv_t<T>>::type, T>;

/**
 * Same as above but also extracts pointer to member from integral_constants.
 */
template <class T>
using uni_member_class_t = std::conditional_t<is_uni_member_ptr_v<T>,
	typename details::member_class_h<
		std::remove_cv_t<uni_integral_base_t<T>>>::type, T>;

/**
 * Safe getter that for pointer to member returns member type and original
 * type in any other case.
 */
namespace details {
template <class T> struct demember_h { using type = T; };
template <class T, class U> struct demember_h<T U::*> { using type = T; };
} //namespace details {

template <class T>
using demember_t = std::conditional_t<is_member_ptr_v<T>,
	typename details::demember_h<std::remove_cv_t<T>>::type, T>;

/**
 * Same as above but also extracts pointer to member from integral_constants.
 */
template <class T>
using uni_demember_t = std::conditional_t<is_uni_member_ptr_v<T>,
	typename details::demember_h<
		std::remove_cv_t<uni_integral_base_t<T>>>::type, T>;

/**
 * Universal value extractor. Return static value member for
 * std::integral_constant, or the value itself otherwise.
 */
template <class T, class U>
auto& uni_member([[maybe_unused]] T& object, U&& uni_member)
{
	if constexpr (is_uni_member_ptr_v<U>)
		return object.*uni_value(uni_member);
	else
		return uni_member;
}

} // namespace tnt {
