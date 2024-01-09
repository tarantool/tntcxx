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

#include "../src/Utils/Ring.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

#include "Utils/Helpers.hpp"

using num_t = uint64_t;

struct Test : tnt::Ring {
	num_t m_Num;
	explicit Test(num_t aNum = 0) : Ring(0), m_Num(aNum) {}
};

template <class T>
void checkRing(const Test* aRing, const T& aList)
{
	fail_unless(aRing->rgSelfCheck() ==  0);
	fail_unless(aRing->rgCalcSize() == aList.size());
	fail_unless(aRing->rgIsMono() == (aList.size() == 1));

	const Test *t = aRing;
	for (num_t val : aList) {
		fail_unless(t->m_Num == val);
		t = static_cast<const Test *>(t->rgNeigh());
	}
	fail_unless(t == aRing);
}

void checkRing(const Test *aRing, const std::initializer_list<num_t> &aList)
{
	return checkRing<std::initializer_list<num_t>>(aRing, aList);
}

static void simple()
{
	TEST_INIT(0);
	{
		// Direct initialization.
		tnt::Ring r;
		r.rgInit();
		fail_unless(r.rgSelfCheck() == 0);
		fail_unless(r.rgIsMono());
		fail_unless(r.rgCalcSize() == 1);
	}
	{
		// Indirect initialization.
		tnt::Ring r(0);
		fail_unless(r.rgSelfCheck() == 0);
		fail_unless(r.rgIsMono());
		fail_unless(r.rgCalcSize() == 1);
	}

	for (bool back: {false, true}) {
		// Add unitialized.
		tnt::Ring r1(0);
		tnt::Ring r2;
		r1.rgAdd(&r2, back);

		fail_unless(r1.rgSelfCheck() == 0);
		fail_unless(r2.rgSelfCheck() == 0);
		fail_unless(r1.rgCalcSize() == 2);
		fail_unless(r1.rgCalcSize() == 2);
	}
	for (bool back: {false, true}) {
		// Initialization by addition to a ring.
		tnt::Ring r1(0);
		tnt::Ring r2(r1, back);

		fail_unless(r1.rgSelfCheck() == 0);
		fail_unless(r2.rgSelfCheck() == 0);
		fail_unless(r1.rgCalcSize() == 2);
		fail_unless(r1.rgCalcSize() == 2);
	}
	for (bool back: {false, true}) {
		// Add self if case of monoring (should do nothing).
		tnt::Ring r(0);
		r.rgAdd(&r, back);

		fail_unless(r.rgSelfCheck() == 0);
		fail_unless(r.rgCalcSize() == 1);
	}
	{
	}
	{
		// Remove self if case of monoring (should do nothing).
		tnt::Ring r(0);
		r.rgRemove();

		fail_unless(r.rgSelfCheck() == 0);
		fail_unless(r.rgCalcSize() == 1);
	}

	{
		Test r(0);
		checkRing(&r, {0});

		Test more[10];
		std::vector<num_t> sComp = {0};
		for (size_t i = 0; i < 10; i++) {
			more[i].m_Num = i + 1;
			r.rgAdd(&more[i], false);
			sComp.insert(sComp.begin() + 1, i + 1);
			checkRing(&r, sComp);
		}
		for (size_t i = 0; i < 10; i++) {
			more[i].rgRemove();
			sComp.pop_back();
			checkRing(&r, sComp);
		}
	}
	{
		Test r(0);
		checkRing(&r, {0});

		Test more[10];
		std::vector<num_t> sComp = {0};
		for (size_t i = 0; i < 10; i++) {
			more[i].m_Num = i + 1;
			r.rgAdd(&more[i], true);
			sComp.push_back(i + 1);
			checkRing(&r, sComp);
		}
		for (size_t i = 0; i < 10; i++) {
			more[i].rgRemove();
			sComp.erase(sComp.begin() + 1);
			checkRing(&r, sComp);
		}
	}
}

template <size_t SIZE1, size_t SIZE2>
static void test_split_join(bool invert)
{
	TEST_INIT(3, SIZE1, SIZE2, invert);
	std::vector<num_t> list1;
	std::vector<num_t> list2;

	Test r1[SIZE1];
	for (size_t i = 0; i < SIZE1; i++) {
		list1.push_back(i);
		r1[i].rgInit();
		r1[i].m_Num = i;

		// Strange thing. Without the line below I got compile erorr
		// When building RelWithDebInfo build:
		// tntcxx/test/RingUnitTest.cpp: In function ‘int main()’:
		// tntcxx/test/RingUnitTest.cpp:184:19: error: array subscript [0, 0] is outside array bounds of ‘Test [1]’ [-Werror=array-bounds]
		//  184 |    r1[0].rgAdd(&r1[i], true);
		//      |                 ~~^
		if (i >= SIZE1) abort();

		if (i > 0)
			r1[0].rgAdd(&r1[i], true);
	}
	checkRing(&r1[0], list1);

	Test r2[SIZE2];
	for (size_t i = 0; i < SIZE2; i++) {
		list2.push_back(i + SIZE1);
		r2[i].rgInit();
		r2[i].m_Num = i + SIZE1;
		if (i > 0)
			r2[0].rgAdd(&r2[i], true);
	}
	checkRing(&r2[0], list2);

	std::vector<num_t> list_joined;
	if (!invert) {
		for (size_t i = 0; i < SIZE1 + SIZE2; i++)
			list_joined.push_back(i);
	} else {
		list_joined.push_back(0);
		for (size_t i = 1; i < SIZE2; i++)
			list_joined.push_back(i + SIZE1);
		list_joined.push_back(SIZE1);
		for (size_t i = 1; i < SIZE1; i++)
			list_joined.push_back(i);
	}
	r1[0].rgJoin(&r2[0], invert);
	checkRing(&r1[0], list_joined);
	r1[0].rgSplit(&r2[0], invert);

	checkRing(&r1[0], list1);
	checkRing(&r2[0], list2);
}

template <size_t SIZE1, size_t SIZE2>
static void test_swap()
{
	TEST_INIT(2, SIZE1, SIZE2);
	std::vector<num_t> list1;
	std::vector<num_t> list2;
	std::vector<num_t> swap_list1;
	std::vector<num_t> swap_list2;

	Test r1[SIZE1];
	for (size_t i = 0; i < SIZE1; i++) {
		list1.push_back(i);
		if (i == 0)
			swap_list1.insert(swap_list1.begin(), i);
		else
			swap_list2.push_back(i);
		r1[i].rgInit();
		r1[i].m_Num = i;

		// Strange thing. Without the line below I got compile erorr
		// When building RelWithDebInfo build:
		// tntcxx/test/RingUnitTest.cpp: In function ‘int main()’:
		// tntcxx/test/RingUnitTest.cpp:247:19: error: array subscript [0, 0] is outside array bounds of ‘Test [1]’ [-Werror=array-bounds]
		//  247 |    r1[0].rgAdd(&r1[i], true);
		//      |                 ~~^
		if (i >= SIZE1) abort();

		if (i > 0)
			r1[0].rgAdd(&r1[i], true);
	}
	checkRing(&r1[0], list1);

	Test r2[SIZE2];
	for (size_t i = 0; i < SIZE2; i++) {
		list2.push_back(i + SIZE1);
		if (i == 0)
			swap_list2.insert(swap_list2.begin(), i + SIZE1);
		else
			swap_list1.push_back(i + SIZE1);
		r2[i].rgInit();
		r2[i].m_Num = i + SIZE1;
		if (i > 0)
			r2[0].rgAdd(&r2[i], true);
	}
	checkRing(&r2[0], list2);

	r1[0].rgSwap(&r2[0]);
	checkRing(&r1[0], swap_list1);
	checkRing(&r2[0], swap_list2);

	r2[0].rgSwap(&r1[0]);
	checkRing(&r1[0], list1);
	checkRing(&r2[0], list2);
}

int main()
{
	simple();

	test_split_join<3, 3>(false);
	test_split_join<1, 3>(false);
	test_split_join<3, 1>(false);
	test_split_join<1, 1>(false);
	test_split_join<3, 3>(true);
	test_split_join<1, 3>(true);
	test_split_join<3, 1>(true);
	test_split_join<1, 1>(true);
	test_swap<3, 3>();
	test_swap<3, 1>();
	test_swap<1, 3>();
	test_swap<1, 1>();

	return 0;
}
