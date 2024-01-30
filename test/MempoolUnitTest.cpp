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

#include "../src/Utils/Mempool.hpp"
#include "Utils/Helpers.hpp"
#include <iostream>

template <size_t S>
struct Allocation {
	char *ptr;
	size_t id;
	void assign(char *alloc_ptr)
	{
		static size_t inc_id = 0;
		ptr = alloc_ptr;
		id = ++inc_id;
		char v = static_cast<char>(id);
		for (size_t i = 0; i < S; i++)
			ptr[i] = v;
	}
	bool is_valid() const
	{
		char v = static_cast<char>(id);
		for (size_t i = 0; i < S; i++)
			if (ptr[i] != v)
				return false;
		return true;
	}
};

template <size_t S, size_t MAX_COUNT>
struct Allocations {
	Allocation<S> all[MAX_COUNT];
	size_t count = 0;
	Allocation<S>& operator[](size_t i) { return all[i]; }
	void add(char *alloc_ptr) { all[count++].assign(alloc_ptr); }
	bool are_valid() const
	{
		for (size_t i = 0; i < count; i++)
			if (!all[i].is_valid())
				return false;
		return true;
	}
	void del(size_t i)
	{
		--count;
		for (; i < count; i++)
			all[i] = all[i + 1];
	}
};


template<size_t S>
void
test_default()
{
	TEST_INIT(2, S, 0);
	std::cout << "Block for size " << S << " is "
		  << tnt::MempoolInstance<S>::BLOCK_SIZE << std::endl;
	std::cout << "Slab for size " << S << " is "
		  << tnt::MempoolInstance<S>::SLAB_SIZE << std::endl;
	std::cout << "SLAB_ALIGN for size " << S << " is "
		  << tnt::MempoolInstance<S>::SLAB_ALIGN << std::endl;
	constexpr size_t N = 1024;
	tnt::MempoolInstance<S> mp;
	Allocations<S, N> all;
	for (size_t i = 0; i < N; i++)
		all.add(mp.allocate());
	fail_unless(all.are_valid());

	size_t bc = mp.statBlockCount(), sc = mp.statSlabCount();
	fail_unless(bc == SIZE_MAX && sc == SIZE_MAX);
	for (size_t i = 0; i < N; i++) {
		fail_unless(all[i].is_valid());
		mp.deallocate(all[i].ptr);
	}
	bc = mp.statBlockCount(), sc = mp.statSlabCount();
	fail_unless(bc == SIZE_MAX && sc == SIZE_MAX);
}


template<size_t S, size_t M>
void
test_instance()
{
	TEST_INIT(2, S, M);
	using mp_t = tnt::MempoolInstance<S, M, true>;
	constexpr size_t EXPECT_BLOCKS_IN_SLAB =
		mp_t::SLAB_SIZE / mp_t::BLOCK_SIZE - 1;
	mp_t mp;
	Allocations<S, EXPECT_BLOCKS_IN_SLAB * 2> all;
	fail_unless(mp.statBlockCount() == 0);
	fail_unless(mp.statSlabCount() == 0);

	all.add(mp.allocate());
	fail_unless(mp.statBlockCount() == 1);
	fail_unless(mp.statSlabCount() == 1);
	for (size_t i = 1; i < EXPECT_BLOCKS_IN_SLAB; i++)
		all.add(mp.allocate());
	fail_unless(mp.statBlockCount() == EXPECT_BLOCKS_IN_SLAB);
	fail_unless(mp.statSlabCount() == 1);

	all.add(mp.allocate());
	fail_unless(mp.statBlockCount() == EXPECT_BLOCKS_IN_SLAB + 1);
	fail_unless(mp.statSlabCount() == 2);
	for (size_t i = 1; i < EXPECT_BLOCKS_IN_SLAB; i++)
		all.add(mp.allocate());
	fail_unless(mp.statBlockCount() == EXPECT_BLOCKS_IN_SLAB * 2);
	fail_unless(mp.statSlabCount() == 2);
	fail_unless(all.are_valid());
	fail_unless(mp.selfcheck() == 0);

	for (size_t i = 0; i < 2 * EXPECT_BLOCKS_IN_SLAB; i++) {
		for (size_t j = 0; j < i; j++) {
			size_t k = static_cast<size_t>(rand()) % all.count;
			fail_unless(all[k].is_valid());
			mp.deallocate(all[k].ptr);
			all.del(k);
			fail_unless(mp.statBlockCount() ==
				    2 * EXPECT_BLOCKS_IN_SLAB - j - 1);
			fail_unless(mp.statSlabCount() == 2);
		}
		fail_unless(all.are_valid());
		fail_unless(mp.selfcheck() == 0);
		for (size_t j = 0; j < i; j++) {
			all.add(mp.allocate());
			fail_unless(mp.statBlockCount() ==
				    2 * EXPECT_BLOCKS_IN_SLAB - i + j + 1);
			fail_unless(mp.statSlabCount() == 2);
		}
		fail_unless(all.are_valid());
		fail_unless(mp.selfcheck() == 0);
	}
}

template<size_t S, size_t M>
void
test_holder()
{
	TEST_INIT(2, S, M);

	using mp_t = tnt::MempoolInstance<S, M, true>;
	using mph_t = tnt::MempoolHolder<S, M, true>;

	mp_t mp;
	mph_t mh1(mp);
	mph_t mh2(mp);
	mph_t mh3(mh1);
	mph_t mh4;
	mph_t mh5(mh4);
	char *a = mh1.allocate();
	char *b = mh2.allocate();
	char *c = mh3.allocate();
	char *d = mh4.allocate();
	char *e = mh5.allocate();

	fail_unless(mp.statBlockCount() == 3);
	fail_unless(mp_t::defaultInstance().statBlockCount() == 2);
	fail_unless(mh1.statBlockCount() == 3);
	fail_unless(mh2.statBlockCount() == 3);
	fail_unless(mh3.statBlockCount() == 3);
	fail_unless(mh4.statBlockCount() == 2);

	mh1.deallocate(a);
	mh2.deallocate(b);
	mh3.deallocate(c);
	mh4.deallocate(d);
	mh5.deallocate(e);

	fail_unless(mp.statBlockCount() == 0);
	fail_unless(mp_t::defaultInstance().statBlockCount() == 0);
	fail_unless(mh1.statBlockCount() == 0);
	fail_unless(mh2.statBlockCount() == 0);
	fail_unless(mh3.statBlockCount() == 0);
	fail_unless(mh4.statBlockCount() == 0);
	fail_unless(mh5.statBlockCount() == 0);
}

template<size_t S, size_t M>
void
test_static()
{
	TEST_INIT(2, S, M);

	using mp_t = tnt::MempoolInstance<S, M, true>;
	using mph_t = tnt::MempoolStatic<S, M, true>;

	char *a = mph_t::allocate();
	fail_unless(mp_t::defaultInstance().statBlockCount() == 1);
	char *b = mph_t::allocate();
	fail_unless(mp_t::defaultInstance().statBlockCount() == 2);
	char *c = mph_t::allocate();
	fail_unless(mp_t::defaultInstance().statBlockCount() == 3);

	mph_t::deallocate(a);
	fail_unless(mp_t::defaultInstance().statBlockCount() == 2);
	mph_t::deallocate(b);
	fail_unless(mp_t::defaultInstance().statBlockCount() == 1);
	mph_t::deallocate(c);
	fail_unless(mp_t::defaultInstance().statBlockCount() == 0);
}

template<size_t S, size_t M>
void
test_alignment()
{
	TEST_INIT(2, S, M);

	size_t alignment = 1;
	while (S % (alignment * 2) == 0)
		alignment *= 2;

	tnt::MempoolInstance<S, M, true> mp;

	for (size_t i = 0; i < 100; i++) {
		char *p = mp.allocate();
		uintptr_t u = (uintptr_t)p;
		fail_unless(u % alignment == 0);
	}
}

int main()
{
	test_default<8>();
	test_default<64>();
	test_default<14>();
	test_default<72>();
	test_default<80>();

	test_instance<8, 256>();
	test_instance<14, 64>();
	test_instance<15, 64>();
	test_instance<63, 32>();
	test_instance<64, 32>();
	test_instance<65, 32>();
	test_instance<80, 8>();

	test_holder<16, 256>();

	test_static<16, 256>();

	test_alignment<8, 2>();
	test_alignment<8, 13>();
	test_alignment<8, 64>();
	test_alignment<13, 8>();
	test_alignment<13, 64>();
	test_alignment<120, 2>();
	test_alignment<120, 13>();
	test_alignment<120, 64>();
}
