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

#include "../src/Utils/RefVector.hpp"
#include "Utils/Helpers.hpp"

#include <cstdint>

void
test_simple()
{
	short data1[5] = {1, 2, 3, 4, 5};
	int size1 = 3;
	auto arr1 = tnt::make_ref_vector(data1, size1);
	static_assert(tnt::is_ref_vector_v<decltype(arr1)>);
	static_assert(tnt::is_ref_vector_v<const decltype(arr1)>);
	static_assert(!tnt::is_ref_vector_v<decltype(data1)>);
	static_assert(!tnt::is_ref_vector_v<decltype(size1)>);
	static_assert(!tnt::is_ref_vector_v<std::array<int, 1>>);
	static_assert(std::is_same_v<int, decltype(arr1.size())>);
	static_assert(std::is_same_v<short&, decltype(arr1[0])>);
	static_assert(arr1.capacity() == 5);
	static_assert(tnt::is_limited_v<decltype(arr1)>);
	static_assert(arr1.static_capacity == 5);
	fail_unless(arr1.size() == 3);

	arr1[0] = 100;
	fail_unless(data1[0] == 100);
	data1[0] = 10;
	fail_unless(arr1[0] == 10);

	std::array<int, 10> data2 = {0};
	size_t size2 = 1;
	auto arr2 = tnt::make_ref_vector(data2, size2);
	static_assert(std::is_same_v<size_t, decltype(arr2.size())>);
	static_assert(std::is_same_v<int&, decltype(arr2[0])>);
	static_assert(arr2.capacity() == 10);
	static_assert(arr2.static_capacity == 10);
	fail_unless(arr2.size() == 1);
	arr2.clear();
	fail_unless(arr2.size() == 0);

	for (short& x : arr1)
		arr2.push_back(x);
	fail_unless(size_t(arr2.size()) == size_t(arr1.size()));
	fail_unless(data2[0] == data1[0]);
	fail_unless(data2[1] == data1[1]);
	fail_unless(data2[2] == data1[2]);

	for (auto itr = arr1.cbegin(); itr != arr1.cend(); ++itr) {
		int x = *itr;
		arr2.push_back(x);
	}
	fail_unless(size_t(arr2.size()) == 2 * size_t(arr1.size()));
	fail_unless(data2[3] == data1[0]);
	fail_unless(data2[4] == data1[1]);
	fail_unless(data2[5] == data1[2]);

	for (auto itr = arr1.cbegin(); itr != arr1.cend(); ++itr) {
		int x = *itr;
		fail_unless(arr2.emplace_back(x) == x);
	}
	fail_unless(size_t(arr2.size()) == 3 * size_t(arr1.size()));
	fail_unless(data2[6] == data1[0]);
	fail_unless(data2[7] == data1[1]);
	fail_unless(data2[8] == data1[2]);

	fail_unless(arr1.data() == &data1[0]);
	fail_unless(arr2.data() == &data2[0]);

	int data3[5];
	int size3 = 0;
	auto arr3 = tnt::make_ref_vector(data3, size3);
	const auto &carr3 = arr3;
	for (int i = 0; i < 5; i++) {
		arr3.push_back(i);
		fail_unless(&arr3.front() == &data3[0]);
		fail_unless(&arr3.back() == &data3[i]);
		fail_unless(&carr3.front() == &data3[0]);
		fail_unless(&carr3.back() == &data3[i]);
	}
	arr3.clear();
	for (int i = 0; i < 5; i++) {
		arr3.push_back(std::move(i));
		fail_unless(&arr3.front() == &data3[0]);
		fail_unless(&arr3.back() == &data3[i]);
		fail_unless(&carr3.front() == &data3[0]);
		fail_unless(&carr3.back() == &data3[i]);
	}
	arr3.resize(3);
	fail_unless(arr3.size() == 3);
	fail_unless(arr3.back() == 2);
	arr3.resize(0);
	fail_unless(size3 == 0);
	for (int i = 0; i < 5; i++) {
		auto &ref = arr3.emplace_back(i);
		fail_unless(&ref == &data3[i]);
		fail_unless(&arr3.front() == &data3[0]);
		fail_unless(&arr3.back() == &data3[i]);
		fail_unless(&carr3.front() == &data3[0]);
		fail_unless(&carr3.back() == &data3[i]);
	}
	arr3.clear();
}

void
test_const()
{
	const short data1[5] = {1, 2, 3, 4, 5};
	int size1 = 3;
	auto arr1 = tnt::make_ref_vector(data1, size1);
	static_assert(std::is_same_v<int, decltype(arr1.size())>);
	static_assert(std::is_same_v<const short&, decltype(arr1[0])>);
	static_assert(arr1.capacity() == 5);
	static_assert(arr1.static_capacity == 5);
	fail_unless(arr1.size() == 3);

	// Expected to fail to build:
	// arr1[0] = 100;
	// arr1.clear();
	// arr1.push_back(1);
	// arr1.emplace_back(1);
	// for (short& x : arr1) {
	size_t s = 0;
	for (const short& x : arr1)
		fail_unless(x == data1[s++]);
	fail_unless(s == size_t(size1));

	std::array<const short, 6> data2 = {1, 2, 3, 4, 5, 6};
	int size2 = 4;
	auto arr2 = tnt::make_ref_vector(data2, size2);
	static_assert(std::is_same_v<int, decltype(arr2.size())>);
	static_assert(std::is_same_v<const short&, decltype(arr2[0])>);
	static_assert(arr2.capacity() == 6);
	static_assert(arr2.static_capacity == 6);
	fail_unless(arr2.size() == 4);

	// Expected to fail to build:
//	 arr2[0] = 200;
//	 arr2.clear();
//	 arr2.push_back(2);
//	 arr2.emplace_back(2);
//	 for (short& x : arr2) {
	s = 0;
	for (const short& x : arr2)
		fail_unless(x == data2[s++]);
	fail_unless(s == size_t(size2));

	int data3[10] = {0, 30};
	const size_t size3 = 3;
	auto arr3 = tnt::make_ref_vector(data3, size3);
	static_assert(std::is_same_v<size_t, decltype(arr3.size())>);
	static_assert(std::is_same_v<int&, decltype(arr3[0])>);
	static_assert(arr3.capacity() == 10);
	static_assert(arr3.static_capacity == 10);
	fail_unless(arr3.size() == 3);

	arr3[0] = 10;
	// Expected to fail to build:
	// arr3.clear();
	// arr3.push_back(1);
	// arr3.emplace_back(1);
	// for (short& x : arr1) {

	s = 0;
	for (auto itr = arr3.cbegin(); itr != arr3.cend(); ++itr)
		fail_unless(*itr == data3[s++]);
	fail_unless(s == size_t(size3));
}

void
test_members_c()
{
	struct TestStruct {
		int data[10];
		int size;
	} t1, t2;
	t1.data[0] = 1;
	t1.data[1] = 2;
	t1.size = 2;
	t2.data[0] = 3;
	t2.size = 1;

	auto data_ptr = &TestStruct::data;
	auto size_ptr = &TestStruct::size;

	auto arr1 = tnt::make_ref_vector(&TestStruct::data, &TestStruct::size);
	auto arr2 = tnt::make_ref_vector(data_ptr, size_ptr);

	static_assert(arr1.static_capacity == 10);
	static_assert(arr2.capacity() == 10);

	// Any access is expected to fail to build:
	// arr1.size();

	auto arr3 = tnt::subst(arr1, t1);
	auto arr4 = tnt::subst(arr2, t2);

	fail_unless(arr3.size() == 2);
	fail_unless(&arr3[0] == &t1.data[0]);
	fail_unless(&arr3[1] == &t1.data[1]);
	fail_unless(arr4.size() == 1);
	fail_unless(&arr4[0] == &t2.data[0]);

	auto arr5 = tnt::subst(arr3, t2);
	fail_unless(arr5.size() == 2);
	fail_unless(&arr5[0] == &t1.data[0]);
	fail_unless(&arr5[1] == &t1.data[1]);

	t1.size = 3;
	fail_unless(arr3.size() == 3);
	fail_unless(arr5.size() == 3);

	t1.data[0] = 1;
	t1.data[1] = 2;
	t1.size = 1;
	uint64_t alt_data[5] = {101, 102, 103, 104, 105};
	uint64_t alt_size = 2;
	auto arr10 = tnt::make_ref_vector(data_ptr, alt_size);
	auto arr11 = tnt::make_ref_vector(alt_data, size_ptr);
	auto arr12 = tnt::subst(arr10, t1);
	auto arr13 = tnt::subst(arr11, t1);
	fail_unless(arr12.size() == 2);
	fail_unless(arr13.size() == 1);
	alt_size = 12;
	t1.size = 11;
	fail_unless(arr12.size() == 12);
	fail_unless(arr13.size() == 11);
	fail_unless(arr12.data() == &t1.data[0]);
	fail_unless(arr13.data() == &alt_data[0]);
}

void
test_members_std()
{
	struct TestStruct {
		std::array<int, 10> data;
		int size;
	} t1, t2;
	t1.data[0] = 1;
	t1.data[1] = 2;
	t1.size = 2;
	t2.data[0] = 3;
	t2.size = 1;

	auto data_ptr = &TestStruct::data;
	auto size_ptr = &TestStruct::size;

	auto arr1 = tnt::make_ref_vector(&TestStruct::data, &TestStruct::size);
	auto arr2 = tnt::make_ref_vector(data_ptr, size_ptr);

	static_assert(arr1.static_capacity == 10);
	static_assert(arr2.capacity() == 10);

	// Any access is expected to fail to build:
	// arr1.size();

	auto arr3 = tnt::subst(arr1, t1);
	auto arr4 = tnt::subst(arr2, t2);

	fail_unless(arr3.size() == 2);
	fail_unless(&arr3[0] == &t1.data[0]);
	fail_unless(&arr3[1] == &t1.data[1]);
	fail_unless(arr4.size() == 1);
	fail_unless(&arr4[0] == &t2.data[0]);

	auto arr5 = tnt::subst(arr3, t2);
	fail_unless(arr5.size() == 2);
	fail_unless(&arr5[0] == &t1.data[0]);
	fail_unless(&arr5[1] == &t1.data[1]);

	t1.size = 3;
	fail_unless(arr3.size() == 3);
	fail_unless(arr5.size() == 3);

	t1.data[0] = 1;
	t1.data[1] = 2;
	t1.size = 1;
	uint64_t alt_data[5] = {101, 102, 103, 104, 105};
	uint64_t alt_size = 2;
	auto arr10 = tnt::make_ref_vector(data_ptr, alt_size);
	auto arr11 = tnt::make_ref_vector(alt_data, size_ptr);
	auto arr12 = tnt::subst(arr10, t1);
	auto arr13 = tnt::subst(arr11, t1);
	fail_unless(arr12.size() == 2);
	fail_unless(arr13.size() == 1);
	alt_size = 12;
	t1.size = 11;
	fail_unless(arr12.size() == 12);
	fail_unless(arr13.size() == 11);
	fail_unless(arr12.data() == &t1.data[0]);
	fail_unless(arr13.data() == &alt_data[0]);
}

int
main()
{
	test_simple();
	test_const();
	test_members_c();
	test_members_std();
}
