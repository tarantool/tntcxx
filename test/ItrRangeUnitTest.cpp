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

#include "../src/Utils/ItrRange.hpp"
#include "Utils/Helpers.hpp"
#include <vector>

void
test_simple()
{
	char buf[10];

	{
		auto range = tnt::make_itr_range(std::begin(buf), std::end(buf));
		using range_t = decltype(range);
		static_assert(tnt::is_itr_range_v<range_t>);
		static_assert(tnt::is_itr_range_v<const range_t>);
		static_assert(!tnt::is_itr_range_v<range_t&>);
		static_assert(!tnt::is_itr_range_v<int>);
		static_assert(!tnt::is_itr_range_v<std::vector<int>>);
		static_assert(std::is_same_v<char*, range_t::itr1_t>);
		static_assert(std::is_same_v<char*, range_t::itr2_t>);
		fail_unless(range.begin() == buf);
		fail_unless(range.end() == buf + std::size(buf));
		range.itr1++;
		range.itr2--;
		fail_unless(range.begin() == buf + 1);
		fail_unless(range.end() == buf + std::size(buf) - 1);
	}

	{
		char *b = std::begin(buf);
		char *e = std::end(buf);
		auto range = tnt::make_itr_range(b, e);
		using range_t = decltype(range);
		static_assert(std::is_same_v<char*&, range_t::itr1_t>);
		static_assert(std::is_same_v<char*&, range_t::itr2_t>);
		fail_unless(range.begin() == buf);
		fail_unless(range.end() == buf + std::size(buf));
		range.itr1++;
		range.itr2--;
		fail_unless(range.begin() == buf + 1);
		fail_unless(range.end() == buf + std::size(buf) - 1);
		fail_unless(b == buf + 1);
		fail_unless(e == buf + std::size(buf) - 1);
	}
}

void
test_members()
{
	char buf[10];
	struct Test {
		char *b = nullptr;
		char *e = nullptr;
	} t;
	auto bptr = &Test::b;
	auto range = tnt::make_itr_range(bptr, &Test::e);
	using range_t = decltype(range);
	static_assert(std::is_same_v<char* Test::*, range_t::itr1_t>);
	static_assert(std::is_same_v<char* Test::*, range_t::itr2_t>);

	auto range2 = tnt::subst(range, t);
	using range2_t = decltype(range2);
	static_assert(std::is_same_v<char*&, range2_t::itr1_t>);
	static_assert(std::is_same_v<char*&, range2_t::itr2_t>);
	range2.itr1 = std::begin(buf);
	range2.itr2 = std::end(buf);
	fail_unless(t.b == buf);
	fail_unless(t.e == buf + std::size(buf));
	fail_unless(range2.begin() == buf);
	fail_unless(range2.end() == buf + std::size(buf));
}

int
main()
{
	test_simple();
}