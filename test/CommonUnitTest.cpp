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

#include "../src/Utils/Common.hpp"
#include "Utils/Helpers.hpp"

void
test_tuple_utils()
{
	TEST_INIT(0);

	{
		using full = std::tuple<int>;
		using cut = tnt::tuple_cut_t<full>;
		using expected = std::tuple<>;
		static_assert(std::is_same_v<cut, expected>);
		static_assert(std::is_same_v<tnt::first_t<full>, int>);
		static_assert(std::is_same_v<tnt::last_t<full>, int>);
	}
	{
		using full = std::tuple<int, float>;
		using cut = tnt::tuple_cut_t<full>;
		using expected = std::tuple<float>;
		static_assert(std::is_same_v<cut, expected>);
		static_assert(std::is_same_v<tnt::first_t<full>, int>);
		static_assert(std::is_same_v<tnt::last_t<full>, float>);
	}
	{
		using full = std::tuple<int, float, double>;
		using cut = tnt::tuple_cut_t<full>;
		using expected = std::tuple<float, double>;
		static_assert(std::is_same_v<cut, expected>);
		static_assert(std::is_same_v<tnt::first_t<full>, int>);
		static_assert(std::is_same_v<tnt::last_t<full>, double>);
	}

	{
		using T = std::tuple<char, uint64_t, uint32_t>;
		static_assert(tnt::tuple_find_v<char, T> == 0);
		static_assert(tnt::tuple_find_v<uint64_t, T> == 1);
		static_assert(tnt::tuple_find_v<uint32_t, T> == 2);
		static_assert(tnt::tuple_find_v<float, T> == 3);
	}

	{
		using T = std::tuple<char, uint64_t>;
		static_assert(tnt::tuple_find_size_v<1, T> == 0);
		static_assert(tnt::tuple_find_size_v<8, T> == 1);
	}
	{
		using T = std::tuple<char, uint64_t>;
		static_assert(tnt::tuple_find_size_v<1, T> == 0);
		static_assert(tnt::tuple_find_size_v<8, T> == 1);
		static_assert(tnt::tuple_find_size_v<4, T> == 2);
		static_assert(tnt::tuple_find_size_v<7, T> == 2);
	}
	{
		using T = std::tuple<char, uint64_t, uint32_t>;
		static_assert(tnt::tuple_find_size_v<1, T> == 0);
		static_assert(tnt::tuple_find_size_v<8, T> == 1);
		static_assert(tnt::tuple_find_size_v<4, T> == 2);
		static_assert(tnt::tuple_find_size_v<7, T> == 3);
	}
}

int main()
{
	static_assert(tnt::always_false_v<double> == false);
	static_assert(tnt::always_false_v<int, double> == false);
	static_assert(tnt::always_false_v<> == false);
	test_tuple_utils();
}
