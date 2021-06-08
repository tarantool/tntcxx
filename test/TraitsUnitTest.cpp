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
#include <optional>
#include <functional>

#include "../src/Utils/Traits.hpp"

#include <array>
#include <utility>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>

#include "Utils/Helpers.hpp"

void
test_integer_traits()
{
	TEST_INIT(0);

	enum E1 { V1 = 0, V_BIG = UINT64_MAX };
	enum E2 { V2 = -1 };
	enum E3 : int { V3 = 0 };
	enum class E4 : uint8_t { V4 = 0 };
	struct Test { };

	// base_enum_t
	static_assert(std::is_unsigned_v<tnt::base_enum_t<E1>>);
	static_assert(std::is_signed_v<tnt::base_enum_t<E2>>);
	static_assert(std::is_same_v<int, tnt::base_enum_t<E3>>);
	static_assert(std::is_same_v<uint8_t, tnt::base_enum_t<E4>>);
	static_assert(std::is_unsigned_v<tnt::base_enum_t<const E1>>);
	static_assert(std::is_signed_v<tnt::base_enum_t<const E2>>);
	static_assert(std::is_same_v<int, tnt::base_enum_t<const E3>>);
	static_assert(std::is_same_v<uint8_t, tnt::base_enum_t<const E4>>);
	static_assert(std::is_same_v<int, tnt::base_enum_t<int>>);
	static_assert(std::is_same_v<char, tnt::base_enum_t<char>>);
	static_assert(std::is_same_v<Test, tnt::base_enum_t<Test>>);
	static_assert(std::is_same_v<E1&, tnt::base_enum_t<E1&>>);
	static_assert(std::is_same_v<E2&, tnt::base_enum_t<E2&>>);
	static_assert(std::is_same_v<E3&, tnt::base_enum_t<E3&>>);
	static_assert(std::is_same_v<E4&, tnt::base_enum_t<E4&>>);

	// are integers
	static_assert(tnt::is_integer_v<int>);
	static_assert(tnt::is_integer_v<const int>);
	static_assert(tnt::is_integer_v<char>);
	static_assert(tnt::is_integer_v<signed char>);
	static_assert(tnt::is_integer_v<unsigned char>);
	static_assert(tnt::is_integer_v<E1>);
	static_assert(tnt::is_integer_v<E2>);
	static_assert(tnt::is_integer_v<E3>);
	static_assert(tnt::is_integer_v<E4>);
	static_assert(tnt::is_integer_v<const E1>);
	static_assert(tnt::is_integer_v<const E2>);
	static_assert(tnt::is_integer_v<const E3>);
	static_assert(tnt::is_integer_v<const E4>);

	// are not integers
	static_assert(!tnt::is_integer_v<bool>);
	static_assert(!tnt::is_integer_v<const bool>);
	static_assert(!tnt::is_integer_v<float>);
	static_assert(!tnt::is_integer_v<double>);
	static_assert(!tnt::is_integer_v<int&>);
	static_assert(!tnt::is_integer_v<E1&>);
	static_assert(!tnt::is_integer_v<E2&>);
	static_assert(!tnt::is_integer_v<E3&>);
	static_assert(!tnt::is_integer_v<E4&>);
	static_assert(!tnt::is_integer_v<bool&>);
	static_assert(!tnt::is_integer_v<Test>);
	static_assert(!tnt::is_integer_v<const Test>);
	static_assert(!tnt::is_integer_v<Test&>);

	// is_signed_integer_v..
	static_assert(tnt::is_signed_integer_v<int>);
	static_assert(!tnt::is_signed_integer_v<uint64_t>);
	static_assert(!tnt::is_signed_integer_v<E1>);
	static_assert(tnt::is_signed_integer_v<E2>);
	static_assert(tnt::is_signed_integer_v<E3>);
	static_assert(!tnt::is_signed_integer_v<E4>);

	static_assert(tnt::is_signed_integer_v<const int>);
	static_assert(!tnt::is_signed_integer_v<const uint64_t>);
	static_assert(!tnt::is_signed_integer_v<const E1>);
	static_assert(tnt::is_signed_integer_v<const E2>);
	static_assert(tnt::is_signed_integer_v<const E3>);
	static_assert(!tnt::is_signed_integer_v<const E4>);

	static_assert(!tnt::is_signed_integer_v<Test>);
	static_assert(!tnt::is_signed_integer_v<float>);
	static_assert(!tnt::is_signed_integer_v<const Test>);
	static_assert(!tnt::is_signed_integer_v<const float>);

	// is_unsigned_integer_v..
	static_assert(!tnt::is_unsigned_integer_v<int>);
	static_assert(tnt::is_unsigned_integer_v<uint64_t>);
	static_assert(tnt::is_unsigned_integer_v<E1>);
	static_assert(!tnt::is_unsigned_integer_v<E2>);
	static_assert(!tnt::is_unsigned_integer_v<E3>);
	static_assert(tnt::is_unsigned_integer_v<E4>);

	static_assert(!tnt::is_unsigned_integer_v<const int>);
	static_assert(tnt::is_unsigned_integer_v<const uint64_t>);
	static_assert(tnt::is_unsigned_integer_v<const E1>);
	static_assert(!tnt::is_unsigned_integer_v<const E2>);
	static_assert(!tnt::is_unsigned_integer_v<const E3>);
	static_assert(tnt::is_unsigned_integer_v<const E4>);

	static_assert(!tnt::is_unsigned_integer_v<Test>);
	static_assert(!tnt::is_unsigned_integer_v<float>);
	static_assert(!tnt::is_unsigned_integer_v<const Test>);
	static_assert(!tnt::is_unsigned_integer_v<const float>);
}

void
test_c_traits()
{
	// That's a difference between C and std arrays:
	// std::array has its own cv qualifier while C passes it to values.
	static_assert(std::is_same_v<std::add_const_t<int[3]>,
				     const int[3]>);
	static_assert(std::is_same_v<std::add_const_t<std::array<int, 3>>,
				     const std::array<int, 3>>);

	using carr_t = int[10];
	using cv_carr_t = const volatile carr_t;
	using sarr_t = std::array<int, 11>;
	using cv_sarr_t = const volatile sarr_t;
	enum E { V = 1 };
	struct Test { };
	using const_int = std::integral_constant<int, 0>;

	static_assert(tnt::is_bounded_array_v<carr_t>);
	static_assert(tnt::is_bounded_array_v<cv_carr_t>);
	static_assert(!tnt::is_bounded_array_v<sarr_t>);
	static_assert(!tnt::is_bounded_array_v<cv_sarr_t>);
	static_assert(!tnt::is_bounded_array_v<int>);
	static_assert(!tnt::is_bounded_array_v<E>);
	static_assert(!tnt::is_bounded_array_v<Test>);
	static_assert(!tnt::is_bounded_array_v<const_int>);

	static_assert(tnt::is_char_ptr_v<char *>);
	static_assert(tnt::is_char_ptr_v<const volatile char *>);
	static_assert(tnt::is_char_ptr_v<volatile char *const>);
	static_assert(!tnt::is_char_ptr_v<signed char *>);
	static_assert(!tnt::is_char_ptr_v<unsigned char *>);
	static_assert(!tnt::is_char_ptr_v<char>);
	static_assert(!tnt::is_char_ptr_v<int *>);
	static_assert(!tnt::is_char_ptr_v<char [10]>);
	static_assert(!tnt::is_char_ptr_v<std::array<char, 10>>);
	static_assert(!tnt::is_char_ptr_v<int>);
	static_assert(!tnt::is_char_ptr_v<E>);
	static_assert(!tnt::is_char_ptr_v<Test>);
}

struct MyConstant {
	using value_type = int;
	static constexpr value_type value = 3;
};

struct MyNotConstant1 {
	static constexpr int value = 3;
};

struct MyNotConstant2 {
	using value_type = int;
};

struct MyNotConstant3 {
	using value_type = short;
	static constexpr int value = 3;
};

struct MyNotConstant4 {
	using value_type = int;
	static value_type value;
};

struct MyNotConstant5 {
	using value_type = int;
	const value_type value = 3;
};

struct MyNotConstant6 {
	using value_type = int;
	value_type value() { return 3; }
};

void
test_integral_constant_traits()
{
	enum E { V = 1 };
	struct Test { int i; };
	using const_int = std::integral_constant<int, 0>;
	using const_enum = std::integral_constant<E, V>;
	using const_bool = std::integral_constant<bool, false>;
	using const_ptr = std::integral_constant<int Test::*, &Test::i>;

	static_assert(tnt::is_integral_constant_v<const_int>);
	static_assert(tnt::is_integral_constant_v<const const_int>);
	static_assert(tnt::is_integral_constant_v<const_enum>);
	static_assert(tnt::is_integral_constant_v<const const_enum>);
	static_assert(tnt::is_integral_constant_v<const_bool>);
	static_assert(tnt::is_integral_constant_v<const const_bool>);
	static_assert(tnt::is_integral_constant_v<const_int>);
	static_assert(tnt::is_integral_constant_v<const_ptr>);
	static_assert(tnt::is_integral_constant_v<const const_ptr>);
	static_assert(!tnt::is_integral_constant_v<const_int&>);
	static_assert(!tnt::is_integral_constant_v<int>);
	static_assert(!tnt::is_integral_constant_v<const int>);
	static_assert(!tnt::is_integral_constant_v<float>);
	static_assert(!tnt::is_integral_constant_v<Test>);

	static_assert(tnt::is_integral_constant_v<MyConstant>);
	static_assert(tnt::is_integral_constant_v<const MyConstant>);
	static_assert(!tnt::is_integral_constant_v<MyNotConstant1>);
	static_assert(!tnt::is_integral_constant_v<MyNotConstant2>);
	static_assert(!tnt::is_integral_constant_v<MyNotConstant3>);
	static_assert(!tnt::is_integral_constant_v<MyNotConstant4>);
	static_assert(!tnt::is_integral_constant_v<MyNotConstant5>);
	static_assert(!tnt::is_integral_constant_v<MyNotConstant6>);

	static_assert(std::is_same_v<int, tnt::uni_integral_base_t<int>>);
	static_assert(std::is_same_v<const int, tnt::uni_integral_base_t<const int>>);
	static_assert(std::is_same_v<E, tnt::uni_integral_base_t<E>>);
	static_assert(std::is_same_v<const E, tnt::uni_integral_base_t<const E>>);
	static_assert(std::is_same_v<bool, tnt::uni_integral_base_t<bool>>);
	static_assert(std::is_same_v<const bool, tnt::uni_integral_base_t<const bool>>);
	static_assert(std::is_same_v<const int, tnt::uni_integral_base_t<const_int>>);
	static_assert(std::is_same_v<const int, tnt::uni_integral_base_t<const const_int>>);
	static_assert(std::is_same_v<const E, tnt::uni_integral_base_t<const_enum>>);
	static_assert(std::is_same_v<const E, tnt::uni_integral_base_t<const const_enum>>);
	static_assert(std::is_same_v<const bool, tnt::uni_integral_base_t<const_bool>>);
	static_assert(std::is_same_v<const bool, tnt::uni_integral_base_t<const const_bool>>);
	static_assert(std::is_same_v<Test, tnt::uni_integral_base_t<Test>>);
	static_assert(std::is_same_v<const Test, tnt::uni_integral_base_t<const Test>>);
	static_assert(std::is_same_v<float, tnt::uni_integral_base_t<float>>);
	static_assert(std::is_same_v<const float, tnt::uni_integral_base_t<const float>>);

	static_assert(tnt::uni_value(const_int{}) == 0);
	static_assert(tnt::uni_value(const_enum{}) == V);
	static_assert(tnt::uni_value(const_bool{}) == false);
	fail_unless(tnt::uni_value(1) == 1);
	fail_unless(tnt::uni_value(V) == V);
	fail_unless(tnt::uni_value(true) == true);
	fail_unless(tnt::uni_value(nullptr) == nullptr);
	fail_unless(tnt::uni_value(1.f) == 1.f);
	fail_unless(tnt::uni_value(2.) == 2.);

	static_assert(tnt::is_uni_integral_v<int>);
	static_assert(tnt::is_uni_integral_v<const int>);
	static_assert(!tnt::is_uni_integral_v<int&>);
	static_assert(tnt::is_uni_integral_v<bool>);
	static_assert(tnt::is_uni_integral_v<const bool>);
	static_assert(!tnt::is_uni_integral_v<bool&>);
	static_assert(!tnt::is_uni_integral_v<E>);
	static_assert(!tnt::is_uni_integral_v<const E>);
	static_assert(!tnt::is_uni_integral_v<E&>);
	static_assert(tnt::is_uni_integral_v<const_int>);
	static_assert(tnt::is_uni_integral_v<const const_int>);
	static_assert(!tnt::is_uni_integral_v<const_int&>);
	static_assert(!tnt::is_uni_integral_v<const_enum>);
	static_assert(!tnt::is_uni_integral_v<const const_enum>);
	static_assert(!tnt::is_uni_integral_v<const_enum&>);
	static_assert(tnt::is_uni_integral_v<const_bool>);
	static_assert(tnt::is_uni_integral_v<const const_bool>);
	static_assert(!tnt::is_uni_integral_v<const_bool&>);
	static_assert(!tnt::is_uni_integral_v<Test>);
	static_assert(!tnt::is_uni_integral_v<float>);

	static_assert(tnt::is_uni_integer_v<int>);
	static_assert(tnt::is_uni_integer_v<const int>);
	static_assert(!tnt::is_uni_integer_v<int&>);
	static_assert(!tnt::is_uni_integer_v<bool>);
	static_assert(!tnt::is_uni_integer_v<const bool>);
	static_assert(!tnt::is_uni_integer_v<bool&>);
	static_assert(tnt::is_uni_integer_v<E>);
	static_assert(tnt::is_uni_integer_v<const E>);
	static_assert(!tnt::is_uni_integer_v<E&>);
	static_assert(tnt::is_uni_integer_v<const_int>);
	static_assert(tnt::is_uni_integer_v<const const_int>);
	static_assert(!tnt::is_uni_integer_v<const_int&>);
	static_assert(tnt::is_uni_integer_v<const_enum>);
	static_assert(tnt::is_uni_integer_v<const const_enum>);
	static_assert(!tnt::is_uni_integer_v<const_enum&>);
	static_assert(!tnt::is_uni_integer_v<const_bool>);
	static_assert(!tnt::is_uni_integer_v<const const_bool>);
	static_assert(!tnt::is_uni_integer_v<const_bool&>);
	static_assert(!tnt::is_uni_integer_v<Test>);
	static_assert(!tnt::is_uni_integer_v<float>);

	static_assert(!tnt::is_uni_bool_v<int>);
	static_assert(!tnt::is_uni_bool_v<const int>);
	static_assert(!tnt::is_uni_bool_v<int&>);
	static_assert(tnt::is_uni_bool_v<bool>);
	static_assert(tnt::is_uni_bool_v<const bool>);
	static_assert(!tnt::is_uni_bool_v<bool&>);
	static_assert(!tnt::is_uni_bool_v<E>);
	static_assert(!tnt::is_uni_bool_v<const E>);
	static_assert(!tnt::is_uni_bool_v<E&>);
	static_assert(!tnt::is_uni_bool_v<const_int>);
	static_assert(!tnt::is_uni_bool_v<const const_int>);
	static_assert(!tnt::is_uni_bool_v<const_int&>);
	static_assert(!tnt::is_uni_bool_v<const_enum>);
	static_assert(!tnt::is_uni_bool_v<const const_enum>);
	static_assert(!tnt::is_uni_bool_v<const_enum&>);
	static_assert(tnt::is_uni_bool_v<const_bool>);
	static_assert(tnt::is_uni_bool_v<const const_bool>);
	static_assert(!tnt::is_uni_bool_v<const_bool&>);
	static_assert(!tnt::is_uni_bool_v<Test>);
	static_assert(!tnt::is_uni_bool_v<float>);
}

/**
 * Class for tnt::get tests.
 * Does not provide std::tuple_size and tuple_element specialization in order
 * to make sure that tnt::get works without them.
 */
struct GetableClass {
	int first;
	float second;
	bool third;
	template <class T> T& get()
	{
		return std::get<T&>(std::tie(first, second, third));
	}
	template <class T> constexpr const T& get() const
	{
		return std::get<const T&>(std::tie(first, second, third));
	}
	template <size_t I> auto& get()
	{
		return std::get<I>(std::tie(first, second, third));
	}
	template <size_t I> constexpr const auto& get() const
	{
		return std::get<I>(std::tie(first, second, third));
	}
};

/**
 * Class for tnt::tuple_size and tnt::tuple_element tests.
 * Provides std::tuple_size and tuple_element specialization.
 */
struct TupleClass : GetableClass {
};

namespace std {
template <>
struct tuple_size<TupleClass> : std::integral_constant<size_t, 3> {};
template <size_t I>
struct tuple_element<I, TupleClass> { using type = std::tuple_element_t<I, std::tuple<int, float, bool>>; };
}

void
test_unversal_access()
{
	TEST_INIT(0);

	using T = std::tuple<float, int>;
	using A = long[3];
	using SA = std::array<short, 4>;
	static_assert(tnt::tuple_size_v<T> == 2);
	static_assert(tnt::tuple_size_v<const volatile T> == 2);
	static_assert(tnt::tuple_size_v<A> == 3);
	static_assert(tnt::tuple_size_v<const volatile A> == 3);
	static_assert(tnt::tuple_size_v<SA> == 4);
	static_assert(tnt::tuple_size_v<const volatile SA> == 4);
	static_assert(tnt::tuple_size_v<TupleClass> == 3);
	static_assert(tnt::tuple_size_v<const volatile TupleClass> == 3);

	static_assert(std::is_same_v<float, tnt::tuple_element_t<0, T>>);
	static_assert(std::is_same_v<int, tnt::tuple_element_t<1, T>>);
	static_assert(std::is_same_v<const volatile float, tnt::tuple_element_t<0, const volatile T>>);
	static_assert(std::is_same_v<long, tnt::tuple_element_t<0, A>>);
	static_assert(std::is_same_v<const long, tnt::tuple_element_t<0, const A>>);
	static_assert(std::is_same_v<const volatile long, tnt::tuple_element_t<0, const volatile A>>);
	static_assert(std::is_same_v<short, tnt::tuple_element_t<0, SA>>);
	static_assert(std::is_same_v<const short, tnt::tuple_element_t<0, const SA>>);
	static_assert(std::is_same_v<const volatile short, tnt::tuple_element_t<0, const volatile SA>>);
	static_assert(std::is_same_v<int, tnt::tuple_element_t<0, TupleClass>>);
	static_assert(std::is_same_v<const volatile int, tnt::tuple_element_t<0, const volatile TupleClass>>);
	static_assert(std::is_same_v<float, tnt::tuple_element_t<1, TupleClass>>);
	static_assert(std::is_same_v<const volatile float, tnt::tuple_element_t<1, const volatile TupleClass>>);

	static_assert(tnt::has_get_by_type_v<int, GetableClass>);
	static_assert(tnt::has_get_by_size_v<0, const GetableClass>);
	static_assert(!tnt::has_get_by_type_v<int, T>);
	static_assert(!tnt::has_get_by_size_v<0, const T>);
	static_assert(!tnt::has_get_by_type_v<int, A>);
	static_assert(!tnt::has_get_by_size_v<0, A>);
	static_assert(!tnt::has_get_by_type_v<int, SA>);
	static_assert(!tnt::has_get_by_size_v<0, SA>);

	constexpr GetableClass cgc{1, 1.5, false};
	constexpr std::tuple<int, float> ct{2, 3.14f};
	constexpr int carr[] = {1, 2, 3};
	constexpr std::array<short, 2> cstdarr = {4, 5};

	static_assert(tnt::get<int>(cgc) == 1);
	static_assert(tnt::get<float>(cgc) == 1.5);
	static_assert(tnt::get<bool>(cgc) == false);
	static_assert(tnt::get<0>(cgc) == 1);
	static_assert(tnt::get<1>(cgc) == 1.5);
	static_assert(tnt::get<2>(cgc) == false);

	static_assert(tnt::get<int>(ct) == 2);
	static_assert(tnt::get<float>(ct) == 3.14f);
	static_assert(tnt::get<0>(ct) == 2);
	static_assert(tnt::get<1>(ct) == 3.14f);

	static_assert(tnt::get<0>(carr) == 1);
	static_assert(tnt::get<1>(carr) == 2);
	static_assert(tnt::get<2>(carr) == 3);

	static_assert(tnt::get<0>(cstdarr) == 4);
	static_assert(tnt::get<1>(cstdarr) == 5);

	GetableClass gc{1, 1.5, false};
	std::tuple<int, float> t{2, 3.14f};
	int arr[] = {1, 2, 3};
	std::array<short, 2> stdarr = {4, 5};

	fail_unless(tnt::get<int>(gc) == 1);
	fail_unless(tnt::get<float>(gc) == 1.5);
	tnt::get<float>(gc) = 2.5;
	fail_unless(tnt::get<float>(gc) == 2.5);
	fail_unless(tnt::get<bool>(gc) == false);
	fail_unless(tnt::get<0>(gc) == 1);
	tnt::get<0>(gc) += 2;
	fail_unless(tnt::get<0>(gc) == 3);
	fail_unless(tnt::get<1>(gc) == 2.5);
	fail_unless(tnt::get<2>(gc) == false);
	tnt::get<2>(gc) = true;
	fail_unless(tnt::get<2>(gc) == true);
	tnt::get<bool>(gc) = false;
	fail_unless(tnt::get<2>(gc) == false);

	fail_unless(tnt::get<int>(t) == 2);
	fail_unless(tnt::get<float>(t) == 3.14f);
	tnt::get<float>(t) = 4.f;
	fail_unless(tnt::get<float>(t) == 4.f);
	fail_unless(tnt::get<0>(t) == 2);
	tnt::get<0>(t)++;
	fail_unless(tnt::get<0>(t) == 3);
	fail_unless(tnt::get<1>(t) == 4.f);

	fail_unless(tnt::get<0>(arr) == 1);
	fail_unless(tnt::get<1>(arr) == 2);
	tnt::get<1>(arr) += 8;
	fail_unless(tnt::get<1>(arr) == 10);
	fail_unless(tnt::get<2>(arr) == 3);

	fail_unless(tnt::get<0>(stdarr) == 4);
	fail_unless(tnt::get<1>(stdarr) == 5);
	tnt::get<1>(stdarr) += 5;
	fail_unless(tnt::get<1>(stdarr) == 10);

	TupleClass tc;
	tnt::get<0>(tc) = 5;
	fail_unless(tnt::get<0>(tc) == 5);
	tnt::get<float>(tc) = 3.5f;
	fail_unless(tnt::get<float>(tc) == 3.5f);
}

struct CustomPair {
	int first;
	double second;
	template <size_t I> auto& get()
	{
		return std::get<I>(std::tie(first, second));
	}
	template <size_t I> constexpr const auto& get() const
	{
		return std::get<I>(std::tie(first, second));
	}
};

namespace std {
template <>
struct tuple_size<CustomPair> : std::integral_constant<size_t, 2> {};
template <size_t I>
struct tuple_element<I, CustomPair> { using type = std::tuple_element_t<I, std::tuple<int, double>>; };
}

void
test_tuple_pair_traits()
{
	enum E { V = 1 };
	struct Test { };
	using const_int = std::integral_constant<int, 0>;

	static_assert(tnt::is_tuplish_v<std::tuple<>>);
	static_assert(tnt::is_tuplish_v<std::tuple<int>>);
	static_assert(tnt::is_tuplish_v<std::tuple<int, int>>);
	static_assert(tnt::is_tuplish_v<std::tuple<int, float, Test, E>>);
	static_assert(tnt::is_tuplish_v<std::tuple<double, float, const char*, std::nullptr_t, bool>>);
	static_assert(tnt::is_tuplish_v<const std::tuple<int, int>>);
	static_assert(tnt::is_tuplish_v<volatile std::tuple<int, int>>);
	static_assert(tnt::is_tuplish_v<std::tuple<std::tuple<>>>);
	static_assert(!tnt::is_tuplish_v<std::tuple<>&>);
	static_assert(!tnt::is_tuplish_v<std::tuple<int>&>);

	static_assert(tnt::is_tuplish_v<std::pair<int, float>>);
	static_assert(tnt::is_tuplish_v<const std::pair<int, float>>);
	static_assert(tnt::is_tuplish_v<volatile std::pair<int, float>>);
	static_assert(!tnt::is_tuplish_v<std::pair<int, float>&>);

	static_assert(tnt::is_tuplish_v<std::array<int, 5>>);
	static_assert(tnt::is_tuplish_v<const std::array<int, 5>>);
	static_assert(!tnt::is_tuplish_v<std::array<int, 5>&>);

	static_assert(tnt::is_tuplish_v<TupleClass>);
	static_assert(tnt::is_tuplish_v<const TupleClass>);
	static_assert(!tnt::is_tuplish_v<const TupleClass&>);

	static_assert(!tnt::is_tuplish_v<Test>);
	static_assert(!tnt::is_tuplish_v<int>);
	static_assert(!tnt::is_tuplish_v<E>);
	static_assert(!tnt::is_tuplish_v<Test>);
	static_assert(!tnt::is_tuplish_v<const_int>);

	static_assert(tnt::is_pairish_v<std::pair<int, int>>);
	static_assert(tnt::is_pairish_v<const std::pair<int, int>>);
	static_assert(tnt::is_pairish_v<std::pair<int&, const float&&>>);
	static_assert(!tnt::is_pairish_v<const std::pair<int, int>&>);
	static_assert(tnt::is_pairish_v<CustomPair>);
	static_assert(tnt::is_pairish_v<const CustomPair>);
	static_assert(!tnt::is_pairish_v<const CustomPair&>);

	static_assert(!tnt::is_pairish_v<TupleClass>);
	static_assert(!tnt::is_pairish_v<std::tuple<int, int>>);
	static_assert(!tnt::is_pairish_v<Test>);
	static_assert(!tnt::is_pairish_v<int>);
	static_assert(!tnt::is_pairish_v<E>);
	static_assert(!tnt::is_pairish_v<Test>);
	static_assert(!tnt::is_pairish_v<const_int>);

	static_assert(tnt::is_tuplish_of_pairish_v<std::tuple<>>);
	static_assert(!tnt::is_tuplish_of_pairish_v<std::tuple<int>>);
	static_assert(tnt::is_tuplish_of_pairish_v<std::tuple<std::pair<int, int>>>);
	static_assert(tnt::is_tuplish_of_pairish_v<std::tuple<std::pair<int, int>, CustomPair>>);
	static_assert(!tnt::is_tuplish_of_pairish_v<std::tuple<int, std::pair<int, int>>>);
	static_assert(!tnt::is_tuplish_of_pairish_v<std::tuple<std::pair<int, int>, int>>);
	static_assert(tnt::is_tuplish_of_pairish_v<std::tuple<std::pair<int, int>, const std::pair<float &, int>>>);
	static_assert(tnt::is_tuplish_of_pairish_v<volatile std::tuple<std::pair<int, int>, std::pair<float&, int>>>);

	static_assert(tnt::is_tuplish_of_pairish_v<const std::tuple<std::pair<int, int>>>);
	static_assert(tnt::is_tuplish_of_pairish_v<volatile std::tuple<std::pair<int, int>>>);
	static_assert(!tnt::is_tuplish_of_pairish_v<std::tuple<>&&>);
	static_assert(!tnt::is_tuplish_of_pairish_v<std::tuple<std::pair<int, int>>&>);
	static_assert(!tnt::is_tuplish_of_pairish_v<std::tuple<std::pair<int, int>&>>);

	static_assert(!tnt::is_tuplish_of_pairish_v<std::array<int, 5>>);
	static_assert(!tnt::is_tuplish_of_pairish_v<int[5]>);
	static_assert(tnt::is_tuplish_of_pairish_v<std::array<std::pair<int, int>, 5>>);
	static_assert(tnt::is_tuplish_of_pairish_v<const std::pair<int, int>[5]>);

	static_assert(!tnt::is_tuplish_of_pairish_v<Test>);
	static_assert(!tnt::is_tuplish_of_pairish_v<int>);
	static_assert(!tnt::is_tuplish_of_pairish_v<E>);
	static_assert(!tnt::is_tuplish_of_pairish_v<Test>);
	static_assert(!tnt::is_tuplish_of_pairish_v<const_int>);
}

struct CustomOptional {
	int i;
	bool has;
	bool has_value() const { return has; }
	operator bool() const { return has; }
	int& value() { return i; }
	int& operator*() { return i; }
};

struct CustomVariant {
	int i;
	double d;
	size_t m_index;
	size_t index() const { return m_index; }
	template <class T> T& get() { return std::get<T&>(std::tie(i, d)); }
	template <class T> const T& get() const { return std::get<const T&>(std::tie(i, d)); }
};

namespace std
{
template<> struct variant_size<CustomVariant> : integral_constant<size_t, 2> {};
template<> struct variant_alternative<0, CustomVariant> { using type = int; };
template<> struct variant_alternative<1, CustomVariant> { using type = double; };
} // namespace std {

void
test_variant_traits()
{
	enum E { V = 1 };
	struct Test { };
	using const_int = std::integral_constant<int, 0>;

	static_assert(tnt::is_variant_v<std::variant<int>>);
	static_assert(tnt::is_variant_v<std::variant<int, float>>);
	static_assert(tnt::is_variant_v<const std::variant<int>>);
	static_assert(tnt::is_variant_v<volatile std::variant<int>>);

	static_assert(tnt::is_variant_v<CustomVariant>);
	static_assert(tnt::is_variant_v<const CustomVariant>);
	static_assert(tnt::is_variant_v<volatile CustomVariant>);

	static_assert(!tnt::is_variant_v<std::variant<int>&>);
	static_assert(!tnt::is_variant_v<std::tuple<int, float>>);
	static_assert(!tnt::is_variant_v<std::optional<int>>);
	static_assert(!tnt::is_variant_v<Test>);
	static_assert(!tnt::is_variant_v<E>);
	static_assert(!tnt::is_variant_v<const_int>);
}

void
test_optional_traits()
{
	enum E { V = 1 };
	struct Test { };
	using const_int = std::integral_constant<int, 0>;

	static_assert(tnt::is_optional_v<std::optional<int>>);
	static_assert(tnt::is_optional_v<const std::optional<int>>);
	static_assert(tnt::is_optional_v<volatile std::optional<int>>);

	static_assert(tnt::is_optional_v<CustomOptional>);
	static_assert(tnt::is_optional_v<const CustomOptional>);
	static_assert(tnt::is_optional_v<volatile CustomOptional>);

	static_assert(!tnt::is_optional_v<std::optional<int>&>);
	static_assert(!tnt::is_optional_v<std::variant<int>>);
	static_assert(!tnt::is_optional_v<std::tuple<int, float>>);
	static_assert(!tnt::is_optional_v<std::variant<int>&>);
	static_assert(!tnt::is_optional_v<Test>);
	static_assert(!tnt::is_optional_v<E>);
	static_assert(!tnt::is_optional_v<const_int>);
}

void
test_member_traits()
{
	struct Test { int i = 2; const int ci = 3; int f(); };
	using member_t = decltype(&Test::i);
	using cmember_t = decltype(&Test::ci);
	using method_t = decltype(&Test::f);
	using icon_member_t = std::integral_constant<member_t, &Test::i>;
	using cicon_member_t = std::integral_constant<cmember_t, &Test::ci>;
	enum E { V = 1 };
	using const_int = std::integral_constant<int, 0>;

	static_assert(tnt::is_member_ptr_v<member_t>);
	static_assert(tnt::is_member_ptr_v<const member_t>);
	static_assert(tnt::is_member_ptr_v<volatile member_t>);
	static_assert(tnt::is_member_ptr_v<cmember_t>);
	static_assert(tnt::is_member_ptr_v<const cmember_t>);
	static_assert(!tnt::is_member_ptr_v<icon_member_t>);
	static_assert(!tnt::is_member_ptr_v<const icon_member_t>);
	static_assert(!tnt::is_member_ptr_v<cicon_member_t>);
	static_assert(!tnt::is_member_ptr_v<const cicon_member_t>);
	static_assert(!tnt::is_member_ptr_v<method_t>);
	static_assert(!tnt::is_member_ptr_v<int>);
	static_assert(!tnt::is_member_ptr_v<const int>);
	static_assert(!tnt::is_member_ptr_v<E>);
	static_assert(!tnt::is_member_ptr_v<const_int>);

	static_assert(tnt::is_uni_member_ptr_v<member_t>);
	static_assert(tnt::is_uni_member_ptr_v<const member_t>);
	static_assert(tnt::is_uni_member_ptr_v<volatile member_t>);
	static_assert(tnt::is_uni_member_ptr_v<cmember_t>);
	static_assert(tnt::is_uni_member_ptr_v<const cmember_t>);
	static_assert(tnt::is_uni_member_ptr_v<icon_member_t>);
	static_assert(tnt::is_uni_member_ptr_v<const icon_member_t>);
	static_assert(tnt::is_uni_member_ptr_v<cicon_member_t>);
	static_assert(tnt::is_uni_member_ptr_v<const cicon_member_t>);
	static_assert(!tnt::is_uni_member_ptr_v<method_t>);
	static_assert(!tnt::is_uni_member_ptr_v<int>);
	static_assert(!tnt::is_uni_member_ptr_v<const int>);
	static_assert(!tnt::is_uni_member_ptr_v<E>);
	static_assert(!tnt::is_uni_member_ptr_v<const_int>);

	static_assert(std::is_same_v<Test, tnt::member_class_t<member_t>>);
	static_assert(std::is_same_v<Test, tnt::member_class_t<const member_t>>);
	static_assert(std::is_same_v<Test, tnt::member_class_t<volatile member_t>>);
	static_assert(std::is_same_v<Test, tnt::member_class_t<cmember_t>>);
	static_assert(std::is_same_v<Test, tnt::member_class_t<const cmember_t>>);
	static_assert(std::is_same_v<method_t, tnt::member_class_t<method_t>>);
	static_assert(std::is_same_v<int, tnt::member_class_t<int>>);
	static_assert(std::is_same_v<const int, tnt::member_class_t<const int>>);
	static_assert(std::is_same_v<E, tnt::member_class_t<E>>);
	static_assert(std::is_same_v<const_int, tnt::member_class_t<const_int>>);
	static_assert(std::is_same_v<icon_member_t, tnt::member_class_t<icon_member_t>>);
	static_assert(std::is_same_v<const icon_member_t, tnt::member_class_t<const icon_member_t>>);
	static_assert(std::is_same_v<cicon_member_t, tnt::member_class_t<cicon_member_t>>);
	static_assert(std::is_same_v<const cicon_member_t, tnt::member_class_t<const cicon_member_t>>);

	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<member_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<const member_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<volatile member_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<cmember_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<const cmember_t>>);
	static_assert(std::is_same_v<method_t, tnt::uni_member_class_t<method_t>>);
	static_assert(std::is_same_v<int, tnt::uni_member_class_t<int>>);
	static_assert(std::is_same_v<const int, tnt::uni_member_class_t<const int>>);
	static_assert(std::is_same_v<E, tnt::uni_member_class_t<E>>);
	static_assert(std::is_same_v<const_int, tnt::uni_member_class_t<const_int>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<icon_member_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<const icon_member_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<cicon_member_t>>);
	static_assert(std::is_same_v<Test, tnt::uni_member_class_t<const cicon_member_t>>);

	static_assert(std::is_same_v<int, tnt::demember_t<member_t>>);
	static_assert(std::is_same_v<int, tnt::demember_t<const member_t>>);
	static_assert(std::is_same_v<int, tnt::demember_t<volatile member_t>>);
	static_assert(std::is_same_v<const int, tnt::demember_t<cmember_t>>);
	static_assert(std::is_same_v<const int, tnt::demember_t<const cmember_t>>);
	static_assert(std::is_same_v<method_t, tnt::demember_t<method_t>>);
	static_assert(std::is_same_v<int, tnt::demember_t<int>>);
	static_assert(std::is_same_v<const int, tnt::demember_t<const int>>);
	static_assert(std::is_same_v<E, tnt::demember_t<E>>);
	static_assert(std::is_same_v<const_int, tnt::demember_t<const_int>>);
	static_assert(std::is_same_v<icon_member_t, tnt::demember_t<icon_member_t>>);
	static_assert(std::is_same_v<const icon_member_t, tnt::demember_t<const icon_member_t>>);
	static_assert(std::is_same_v<cicon_member_t, tnt::demember_t<cicon_member_t>>);
	static_assert(std::is_same_v<const cicon_member_t, tnt::demember_t<const cicon_member_t>>);

	static_assert(std::is_same_v<int, tnt::uni_demember_t<member_t>>);
	static_assert(std::is_same_v<int, tnt::uni_demember_t<const member_t>>);
	static_assert(std::is_same_v<int, tnt::uni_demember_t<volatile member_t>>);
	static_assert(std::is_same_v<const int, tnt::uni_demember_t<cmember_t>>);
	static_assert(std::is_same_v<const int, tnt::uni_demember_t<const cmember_t>>);
	static_assert(std::is_same_v<method_t, tnt::uni_demember_t<method_t>>);
	static_assert(std::is_same_v<int, tnt::uni_demember_t<int>>);
	static_assert(std::is_same_v<const int, tnt::uni_demember_t<const int>>);
	static_assert(std::is_same_v<E, tnt::uni_demember_t<E>>);
	static_assert(std::is_same_v<const_int, tnt::uni_demember_t<const_int>>);
	static_assert(std::is_same_v<int, tnt::uni_demember_t<icon_member_t>>);
	static_assert(std::is_same_v<int, tnt::uni_demember_t<const icon_member_t>>);
	static_assert(std::is_same_v<const int, tnt::uni_demember_t<cicon_member_t>>);
	static_assert(std::is_same_v<const int, tnt::uni_demember_t<const cicon_member_t>>);

	int i = 4;
	Test t;
	static_assert(std::is_same_v<int&, decltype(tnt::uni_member(t, i))>);
	static_assert(std::is_same_v<int&, decltype(tnt::uni_member(t, &Test::i))>);
	static_assert(std::is_same_v<const int&, decltype(tnt::uni_member(t, &Test::ci))>);
	fail_unless(&tnt::uni_member(t, i) == &i);
	fail_unless(&tnt::uni_member(t, &Test::i) == &t.i);
	fail_unless(&tnt::uni_member(t, &Test::ci) == &t.ci);

	const Test ct;
	static_assert(std::is_same_v<int&, decltype(tnt::uni_member(ct, i))>);
	static_assert(std::is_same_v<const int&, decltype(tnt::uni_member(ct, &Test::i))>);
	static_assert(std::is_same_v<const int&, decltype(tnt::uni_member(ct, &Test::ci))>);
	fail_unless(&tnt::uni_member(ct, i) == &i);
	fail_unless(&tnt::uni_member(ct, &Test::i) == &ct.i);
	fail_unless(&tnt::uni_member(ct, &Test::ci) == &ct.ci);
}

struct BrandNewArray {
	int *begin() noexcept;
	int *end() noexcept;
	const int *begin() const noexcept;
	const int *end() const noexcept;
	int *data() noexcept;
	const int *data() const noexcept;
	int size() const noexcept;
	static constexpr size_t static_capacity = 10;
};

struct BrandNewSet {
	int *begin() noexcept;
	int *end() noexcept;
	const int *begin() const noexcept;
	const int *end() const noexcept;
	int size() const noexcept;
};

struct BrandNewMap {
	std::pair<int, int> *begin() noexcept;
	std::pair<int, int> *end() noexcept;
	const std::pair<int, int> *begin() const noexcept;
	const std::pair<int, int> *end() const noexcept;
	int size() const noexcept;
};

template <class CONT, bool SIZABLE, bool CONTIGUOUS, bool CONTIGUOUS_CHAR, bool ITERABLE, bool PAIR_ITERABLE>
void check_cont()
{
	static_assert(tnt::is_sizable_v<CONT> == SIZABLE);
	static_assert(tnt::is_contiguous_v<CONT> == CONTIGUOUS);
	static_assert(tnt::is_contiguous_char_v<CONT> == CONTIGUOUS_CHAR);
	static_assert(tnt::is_const_iterable_v<CONT> == ITERABLE);
	static_assert(tnt::is_const_pairs_iterable_v<CONT> == PAIR_ITERABLE);
}

void
test_container_traits()
{
	check_cont<std::vector<int>, true, true, false, true, false>();
	check_cont<const std::vector<int>, true, true, false, true, false>();
	check_cont<const std::vector<int>&, true, true, false, true, false>();
	check_cont<std::vector<bool>, true, false, false, true, false>();
	check_cont<std::vector<std::pair<int, int>>, true, true, false, true, true>();
	check_cont<std::vector<char>, true, true, true, true, false>();
	check_cont<const std::vector<char>, true, true, true, true, false>();
	check_cont<const std::vector<char>&, true, true, true, true, false>();

	check_cont<int[5], true, true, false, true, false>();
	check_cont<std::array<int, 5>, true, true, false, true, false>();
	check_cont<std::list<int>, true, false, false, true, false>();
	check_cont<std::forward_list<int>, false, false, false, true, false>();
	check_cont<std::deque<int>, true, false, false, true, false>();
	check_cont<std::set<int>, true, false, false, true, false>();
	check_cont<std::unordered_set<int>, true, false, false, true, false>();
	check_cont<std::map<int, int>, true, false, false, true, true>();
	check_cont<std::unordered_map<int, int>, true, false, false, true, true>();
	check_cont<char[5], true, true, true, true, false>();
	check_cont<const char[5], true, true, true, true, false>();
	check_cont<std::array<char, 5>, true, true, true, true, false>();
	check_cont<const std::array<char, 5>, true, true, true, true, false>();
	check_cont<std::array<char, 5>&, true, true, true, true, false>();

	check_cont<BrandNewArray, true, true, false, true, false>();
	check_cont<BrandNewSet, true, false, false, true, false>();
	check_cont<BrandNewMap, true, false, false, true, true>();

	static_assert(tnt::is_limited_v<BrandNewArray>);
	static_assert(tnt::is_limited_v<const BrandNewArray>);
	static_assert(!tnt::is_limited_v<std::vector<int>>);
	static_assert(!tnt::is_limited_v<std::set<int>>);
}

MAKE_IS_METHOD_CALLABLE_CHECKER(set);

struct A1 { void set(int, float); };
struct A2 { void set(float, float); };
struct A3 { void set(int&, float&); };
struct A4 { void set(const int&, const float&); };
struct A5 { void get(int, float); };
struct A6 { template <class T, class U> void set(T, U); };
template <class T>
struct A7 { template <class U> void set(T, U); };

void
test_is_method_callable()
{
	static_assert(is_set_callable_v<A1, int, float>);
	static_assert(!is_set_callable_v<A1, int, void*>);
	static_assert(!is_set_callable_v<A1, int, float, float>);
	static_assert(is_set_callable_v<A1, int&, float&>);
	static_assert(is_set_callable_v<A2, int, float>); // convert int to float.
	static_assert(is_set_callable_v<A2, int&, float&>);
	static_assert(is_set_callable_v<A3, int&, float&>);
	static_assert(!is_set_callable_v<A3, int, float>); // can't convert rvalue to reference.
	static_assert(is_set_callable_v<A4, int&, float&>);
	static_assert(is_set_callable_v<A4, int, float>); // can convert rvalue to const reference.
	static_assert(!is_set_callable_v<A5, int, float>); // no method.
	static_assert(is_set_callable_v<A6, int, float>);
	static_assert(!is_set_callable_v<A6, int, float, int>); // too many args.
	static_assert(is_set_callable_v<A7<int>, int, float>);
	static_assert(!is_set_callable_v<A7<int*>, int, float>); // can't convert.
}

int main()
{
	test_integer_traits();
	test_c_traits();
	test_integral_constant_traits();
	test_unversal_access();
	test_tuple_pair_traits();
	test_variant_traits();
	test_optional_traits();
	test_member_traits();
	test_container_traits();
	test_is_method_callable();
}
