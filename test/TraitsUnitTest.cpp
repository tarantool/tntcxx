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

#include "../src/Utils/Traits.hpp"

#include <array>

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

int main()
{
	test_integer_traits();
	test_c_traits();
}
