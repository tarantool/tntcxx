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

#include "../src/Buffer/Buffer.hpp"

#include <sys/uio.h> /* struct iovec */
#include <iostream>

#include "Utils/Helpers.hpp"

constexpr static size_t SMALL_BLOCK_SZ = 64;
constexpr static size_t LARGE_BLOCK_SZ = 256;

static char char_samples[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

constexpr static int SAMPLES_CNT = sizeof(char_samples);

static int int_sample = 666;

static double double_sample = 66.6;

struct struct_sample {
	int i;
	char c;
	double d;
} struct_sample = {1, '1', 1.1};

static char end_marker = '#';

template<size_t N>
static void
fillBuffer(tnt::Buffer<N> &buffer, size_t size)
{
	for (size_t i = 0; i < size; ++i) {
		buffer.write(char_samples[i % SAMPLES_CNT]);
		fail_if(buffer.debugSelfCheck());
	}
}

template<size_t N>
static void
eraseBuffer(tnt::Buffer<N> &buffer)
{
	int IOVEC_MAX = 1024;
	struct iovec vec[IOVEC_MAX];
	do {
		size_t vec_size = buffer.getIOV(buffer.begin(), vec, IOVEC_MAX);
		buffer.dropFront(vec_size);
		fail_if(buffer.debugSelfCheck());
	} while (!buffer.empty());
}

/**
 * Dump buffer to @output string with human readable format.
 * Not the fastest, but quite elementary implementation.
 */
template<size_t N>
static void
dumpBuffer(tnt::Buffer<N> &buffer, std::string &output)
{
	size_t vec_len = 0;
	int IOVEC_MAX = 1024;
	size_t block_cnt = 0;
	struct iovec vec[IOVEC_MAX];
	for (auto itr = buffer.begin(); itr != buffer.end(); itr += vec_len) {
		size_t vec_cnt = buffer.getIOV(buffer.begin(), vec, IOVEC_MAX);
		for (size_t i = 0; i < vec_cnt; ++i) {
			output.append("|sz=" + std::to_string(vec[i].iov_len) + "|");
			output.append((const char *) vec[i].iov_base,
				      vec[i].iov_len);
			output.append("|");
			vec_len += vec[i].iov_len;
		}
		block_cnt += vec_cnt;
	}
	output.insert(0, "bcnt=" + std::to_string(block_cnt));
}

template<size_t N>
static void
printBuffer(tnt::Buffer<N> &buffer)
{
	std::string str;
	dumpBuffer(buffer, str);
	std::cout << "Buffer:" << str << std::endl;
}

/**
 * AddBack() + dropBack()/dropFront() combinations.
 */
template<size_t N>
void
buffer_basic()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	fail_unless(buf.empty());
	buf.write(int_sample);
	fail_unless(! buf.empty());
	fail_if(buf.debugSelfCheck());
	auto itr = buf.begin();
	int int_res = -1;
	itr.get(int_res);
	fail_unless(int_res == int_sample);
	fail_if(buf.debugSelfCheck());
	itr.unlink();
	buf.dropBack(sizeof(int_sample));
	fail_unless(buf.empty());
	fail_if(buf.debugSelfCheck());
	/* Test non-template ::write() method. */
	buf.write({char_samples, SAMPLES_CNT});
	fail_unless(! buf.empty());
	fail_if(buf.debugSelfCheck());
	char char_res[SAMPLES_CNT];
	itr = buf.begin();
	itr.get({char_res, SAMPLES_CNT});
	for (int i = 0; i < SAMPLES_CNT; ++i)
		fail_unless(char_samples[i] == char_res[i]);
	itr.unlink();
	buf.dropFront(SAMPLES_CNT);
	fail_unless(buf.empty());
	fail_if(buf.debugSelfCheck());
	/* Add double value in buffer. */
	itr = buf.end();
	buf.write({sizeof(double)});
	itr.set(double_sample);
	double double_res = 0;
	itr.get(double_res);
	fail_unless(double_res == double_sample);
	fail_if(buf.debugSelfCheck());
	itr.unlink();
	buf.dropFront(sizeof(double));
	fail_unless(buf.empty());
	fail_if(buf.debugSelfCheck());
	/* Add struct value in buffer. */
	itr = buf.end();
	buf.write({sizeof(struct_sample)});
	itr.set(struct_sample);
	struct struct_sample struct_res = { };
	itr.get(struct_res);
	fail_unless(struct_res.c == struct_sample.c);
	fail_unless(struct_res.i == struct_sample.i);
	fail_unless(struct_res.d == struct_sample.d);
	fail_if(buf.debugSelfCheck());
	itr.unlink();
	buf.dropFront(sizeof(struct_sample));
	fail_unless(buf.empty());
	fail_if(buf.debugSelfCheck());

	// Check dropFront in boundary case
	for (size_t i = 1; i < N; i++) {
		tnt::Buffer<N> buf2;
		char c = '!';
		for (size_t j = 0; j < i; j++) // Add i bytes.
			buf2.write(c);
		buf2.dropFront(i);
		fail_unless(buf2.empty());
	}

	for (size_t i = 1; i < N; i++) {
		tnt::Buffer<N> buf2;
		char c = '!';
		for (size_t j = 0; j <= i; j++) // Add i+1 bytes (notice '<=').
			buf2.write(c);
		buf2.dropFront(i);
		fail_unless(*buf2.begin() == c);
	}

	// Check dropBack in boundary case
	for (size_t i = 1; i < N; i++) {
		tnt::Buffer<N> buf2;
		char c = '!';
		for (size_t j = 0; j < i; j++) // Add i bytes.
			buf2.write(c);
		buf2.dropBack(i);
		fail_unless(buf2.empty());
	}

	for (size_t i = 1; i < N; i++) {
		tnt::Buffer<N> buf2;
		char c = '!';
		for (size_t j = 0; j <= i; j++) // Add i+1 bytes (notice '<=').
			buf2.write(c);
		buf2.dropBack(i);
		fail_unless(*buf2.begin() == c);
	}

	// Check when there a heavy iterators.
	for (size_t i = 0; i < N; i++) {
		tnt::Buffer<N> buf2;
		if (i != 0)
			buf2.write({i});
		itr = buf2.end();
		for (size_t j = 1; j < N; j++) {
			buf2.write({j});
			buf2.dropBack(j);
		}
		fail_unless(buf2.end() == itr);
	}
}

/**
 * AddBack() + read combinations.
 */
template<size_t N>
void
buffer_add_read()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	constexpr size_t NUM_OBJ = 1024;

	srand(0);
	for (size_t i = 0; i < NUM_OBJ; i++) {
		int r = rand();
		switch (r % 5) {
		case 0:
			buf.write(static_cast<uint8_t>(r)); break;
		case 1:
			buf.write(static_cast<uint16_t>(r)); break;
		case 2:
			buf.write(static_cast<uint32_t>(r)); break;
		case 3:
			buf.write(static_cast<uint64_t>(r)); break;
		default: {
			size_t sz = r % 13 + 1;
			char data[16];
			for (size_t j = 0; j < sz; j++)
				data[j] = rand();
			buf.write({data, sz});
		}
		}
	}

	auto itr1 = buf.begin();
	srand(0);
	for (size_t i = 0; i < NUM_OBJ; i++) {
		int r = rand();
		switch (r % 5) {
		case 0:
			fail_unless(itr1.template read<uint8_t>() ==
				    static_cast<uint8_t>(r));
			break;
		case 1:
			fail_unless(itr1.template read<uint16_t>() ==
				    static_cast<uint16_t>(r));
			break;
		case 2:
			fail_unless(itr1.template read<uint32_t>() ==
				    static_cast<uint32_t>(r));
			break;
		case 3:
			fail_unless(itr1.template read<uint64_t>() ==
				    static_cast<uint64_t>(r));
			break;
		default: {
			size_t sz = r % 13 + 1;
			char data1[16];
			for (size_t j = 0; j < sz; j++)
				data1[j] = rand();
			char data2[16];
			itr1.read({data2, sz});
			fail_unless(memcmp(data1, data2, sz) == 0);
		}
		}
	}
	fail_unless(itr1 == buf.template end<false>());

	auto itr2 = buf.template begin<true>();
	srand(0);
	for (size_t i = 0; i < NUM_OBJ; i++) {
		int r = rand();
		switch (r % 5) {
		case 0:
			fail_unless(itr2.template read<uint8_t>() ==
				    static_cast<uint8_t>(r));
			break;
		case 1:
			fail_unless(itr2.template read<uint16_t>() ==
				    static_cast<uint16_t>(r));
			break;
		case 2:
			fail_unless(itr2.template read<uint32_t>() ==
				    static_cast<uint32_t>(r));
			break;
		case 3:
			fail_unless(itr2.template read<uint64_t>() ==
				    static_cast<uint64_t>(r));
			break;
		default: {
			size_t sz = r % 13 + 1;
			char data1[16];
			for (size_t j = 0; j < sz; j++)
				data1[j] = rand();
			char data2[16];
			itr2.read({data2, sz});
			fail_unless(memcmp(data1, data2, sz) == 0);
		}
		}
	}
	fail_unless(itr2 == buf.template end<true>());
}
template<size_t N>
void
buffer_iterator()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	fillBuffer(buf, SAMPLES_CNT);
	buf.write(end_marker);
	fail_if(buf.debugSelfCheck());
	auto itr = buf.begin();
	char res = 'x';
	/* Iterator to the start of buffer should not change. */
	for (int i = 0; i < SAMPLES_CNT; ++i) {
		itr.get(res);
		fail_unless(res == char_samples[i]);
		++itr;
	}
	itr.get(res);
	fail_unless(res == end_marker);
	auto begin = buf.begin();
	while (begin != itr)
		begin += 1;
	res = 'x';
	begin.get(res);
	fail_unless(res == end_marker);
	buf.dropFront(SAMPLES_CNT);
	fail_if(buf.debugSelfCheck());
	auto end = buf.end();
	fail_unless(end != itr);
	fail_unless(end != begin);
	fail_if(buf.debugSelfCheck());
	++itr;
	fail_unless(end == itr);
	itr.unlink();
	begin.unlink();
	end.unlink();
	buf.dropBack(1);
	fail_unless(buf.empty());
	fail_if(buf.debugSelfCheck());

	itr = buf.begin();
	const auto& citr = itr;
	typename tnt::Buffer<N>::iterator itr2(itr);

	auto litr1 = itr.enlight();
	const auto litr2 = citr.enlight();
	auto litr3 = litr1.enlight();
	auto litr4 = litr2.enlight();
	(void)litr1; (void)litr2; (void)litr3; (void)litr4;

	static_assert(sizeof(itr) == 32, "Just for information");
	static_assert(sizeof(itr.enlight()) == 8, "Just for information");
}

template <size_t N>
void
buffer_insert()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	fillBuffer(buf, SAMPLES_CNT);
	fail_if(buf.debugSelfCheck());
	buf.write(end_marker);
	fail_if(buf.debugSelfCheck());
	auto begin = buf.begin();
	auto mid_itr = buf.end();
	auto mid_itr2 = buf.end();
	fillBuffer(buf, SAMPLES_CNT);
	fail_if(buf.debugSelfCheck());
	buf.write(end_marker);
	fail_if(buf.debugSelfCheck());
	auto end_itr = buf.end();
	/* For SMALL_BLOCK_SZ = 32
	 * Buffer:bcnt=3|sz=8|01234567||sz=8|89#01234||sz=6|56789#|
	 *                                      ^
	 *                                   mid_itr
	 * */
	buf.insert(mid_itr, SMALL_BLOCK_SZ / 2);
	char res = 'x';
	mid_itr += SMALL_BLOCK_SZ / 2;
	for (int i = 0; i < SAMPLES_CNT / 2; ++i) {
		mid_itr.get(res);
		fail_unless(res == char_samples[i]);
		fail_if(buf.debugSelfCheck());
		++mid_itr;
	}
	mid_itr2 += SMALL_BLOCK_SZ / 2;
	for (int i = 0; i < SAMPLES_CNT / 2; ++i) {
		mid_itr2.get(res);
		fail_unless(res == char_samples[i]);
		fail_if(buf.debugSelfCheck());
		++mid_itr2;
	}
	begin.unlink();
	mid_itr.unlink();
	mid_itr2.unlink();
	end_itr.unlink();
	eraseBuffer(buf);
	/* Try the same but with more elements in buffer (i.e. more blocks). */
	fillBuffer(buf, SAMPLES_CNT * 2);
	//buf.write(end_marker);
	mid_itr = buf.end();
	fillBuffer(buf, SAMPLES_CNT * 4);
	mid_itr2 = buf.end();
	buf.write(end_marker);
	fillBuffer(buf, SAMPLES_CNT * 4);
	end_itr = buf.end();
	buf.write(end_marker);
	fillBuffer(buf, SAMPLES_CNT * 2);
	buf.write(end_marker);
	buf.insert(mid_itr, SAMPLES_CNT * 3);
	end_itr.get(res);
	fail_unless(res == end_marker);
	mid_itr2.get(res);
	fail_unless(res == end_marker);
	/*
	 * Buffer content prior to the iterator used to process insertion
	 * should remain unchanged.
	 */
	int i = 0;
	for (auto tmp = buf.begin(); tmp < mid_itr; ++tmp) {
		tmp.get(res);
		fail_unless(res == char_samples[i++ % SAMPLES_CNT]);
	}
}

template <size_t N>
void
buffer_release()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	fillBuffer(buf, SAMPLES_CNT);
	buf.write(end_marker);
	auto begin = buf.begin();
	auto mid_itr = buf.end();
	auto mid_itr2 = buf.end();
	fillBuffer(buf, SAMPLES_CNT);
	fail_if(buf.debugSelfCheck());
	buf.write(end_marker);
	fail_if(buf.debugSelfCheck());
	auto end_itr = buf.end();
	/* For SMALL_BLOCK_SZ = 32
	 * Buffer:|sz=8|01234567||sz=8|89#01234||sz=6|56789#|
	 *                                ^                 ^
	 *                              mid_itr          end_itr
	 */
	buf.release(mid_itr, SAMPLES_CNT / 2);
	/* Buffer:|sz=8|01234567||sz=8|89#56789||sz=1|#|
	 *                                ^            ^
	 *                              mid_itr     end_itr
	 */
	char res = 'x';
	for (int i = 0; i < SAMPLES_CNT / 2; ++i) {
		mid_itr.get(res);
		fail_unless(res == char_samples[i + SAMPLES_CNT / 2]);
		++mid_itr;
	}
	for (int i = 0; i < SAMPLES_CNT / 2; ++i) {
		mid_itr2.get(res);
		fail_unless(res == char_samples[i + SAMPLES_CNT / 2]);
		++mid_itr2;
	}
	fail_unless(++mid_itr == end_itr);
	mid_itr.unlink();
	mid_itr2.unlink();
	end_itr.unlink();
	begin.unlink();
	eraseBuffer(buf);
	/* Try the same but with more elements in buffer (i.e. more blocks). */
	fillBuffer(buf, SAMPLES_CNT * 2);
	mid_itr = buf.end();
	fillBuffer(buf, SAMPLES_CNT * 4);
	mid_itr2 = buf.end();
	buf.write(end_marker);
	fillBuffer(buf, SAMPLES_CNT * 4);
	end_itr = buf.end();
	buf.write(end_marker);
	fillBuffer(buf, SAMPLES_CNT * 2);
	buf.write(end_marker);
	fail_if(buf.debugSelfCheck());
	std::string tmp;
	dumpBuffer(buf, tmp);
	std::cout << tmp << std::endl;
	buf.release(mid_itr, SAMPLES_CNT * 3);
	tmp.clear();
	dumpBuffer(buf, tmp);
	std::cout << tmp << std::endl;
	fail_if(buf.debugSelfCheck());
	end_itr.get(res);
	fail_unless(res == end_marker);
	mid_itr2.get(res);
	fail_unless(res == end_marker);
	/*
	 * Buffer content prior to the iterator used to process insertion
	 * should remain unchanged.
	 */
	int i = 0;
	for (auto tmp = buf.begin(); tmp < mid_itr; ++tmp) {
		tmp.get(res);
		fail_unless(res == char_samples[i++ % SAMPLES_CNT]);
	}
}

/**
 * Complex test emulating IPROTO interaction.
 */
template<size_t N>
void
buffer_out()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	buf.write(0xce); // uin32 tag
	auto save = buf.end();
	buf.write({4}); // uint32, will be set later
	buf.write(0x82); // map(2) - header
	buf.write(0x00); // IPROTO_REQUEST_TYPE
	buf.write(0x01); // IPROTO_SELECT
	buf.write(0x01); // IPROTO_SYNC
	buf.write(0x00); // sync = 0
	buf.write(0x82); // map(2) - body
	buf.write(0x10); // IPROTO_SPACE_ID
	buf.write(0xcd); // uint16 tag
	buf.write(__builtin_bswap16(512)); // space_id = 512
	buf.write(0x20); // IPROTO_KEY
	buf.write(0x90); // empty array key
	size_t total = buf.end() - save;
	save.set(__builtin_bswap32(total)); // set calculated size
	fail_if(buf.debugSelfCheck());
	save.unlink();
	do {
		int IOVEC_MAX = 1024;
		struct iovec vec[IOVEC_MAX];
		size_t vec_size = buf.getIOV(buf.begin(), vec, IOVEC_MAX);
		buf.dropFront(vec_size);
		fail_if(buf.debugSelfCheck());
	} while (!buf.empty());
}

/**
 * Test iterator::get() method.
 */
template<size_t N>
void
buffer_iterator_get()
{
	TEST_INIT(1, N);
	tnt::Buffer<N> buf;
	size_t DATA_SIZE = SAMPLES_CNT * 10;
	fillBuffer(buf, DATA_SIZE);
	buf.write(end_marker);
	fail_if(buf.debugSelfCheck());
	char char_res[DATA_SIZE];
	memset(char_res, '0', DATA_SIZE);
	auto itr = buf.begin();
	itr.get({char_res, DATA_SIZE});
	for (size_t i = 0; i < DATA_SIZE; ++i)
		fail_unless(char_samples[i % SAMPLES_CNT] == char_res[i]);
	fail_if(buf.debugSelfCheck());
}

template <class BUF>
auto itr_at(BUF &buf, size_t pos)
{
	auto itr = buf.begin();
	itr += pos;
	return itr;
}

/**
 * Test move constructor and assignment.
 */
template<size_t N>
void
buffer_move()
{
	TEST_INIT(1, N);
	for (size_t S = N / 2; S <= 2 * N; S *= 2)
	{
		tnt::Buffer<N> buf1;
		fillBuffer(buf1, S);
		/* It is ok to move to itself. */
		buf1 = std::move(buf1);

		/* Create three iterators pointing to different parts. */
		auto itr0 = buf1.begin();
		auto itr1 = buf1.begin();
		itr1 += 1;
		auto itr2 = buf1.begin();
		std::advance(itr2, S / 2);
		auto itr3 = buf1.begin();
		std::advance(itr3, S - 1);
		char expect1 = char_samples[1];
		char expect2 = char_samples[(S / 2) % SAMPLES_CNT];
		char expect3 = char_samples[(S - 1) % SAMPLES_CNT];
		size_t ins_cnt = 0;

		fail_unless(itr0 == buf1.begin());
		fail_unless(itr1 == itr_at(buf1, 1 + ins_cnt));
		fail_unless(itr2 == itr_at(buf1, S / 2 + ins_cnt));
		fail_unless(itr3 == itr_at(buf1, S - 1 + ins_cnt));
		fail_unless(*itr1 == expect1);
		fail_unless(*itr2 == expect2);
		fail_unless(*itr3 == expect3);
		fail_unless(buf1.has(S + ins_cnt));
		fail_unless(!buf1.has(S + ins_cnt + 1));
		fail_unless(buf1.has(buf1.begin(), S + ins_cnt));
		fail_unless(!buf1.has(buf1.begin(), S + ins_cnt + 1));
		fail_unless(buf1.begin().has(S + ins_cnt));
		fail_unless(!buf1.begin().has(S + ins_cnt + 1));

		buf1.flush();
		fail_unless(buf1.has(S + ins_cnt));
		fail_unless(!buf1.has(S + ins_cnt + 1));
		fail_unless(buf1.has(buf1.begin(), S + ins_cnt));
		fail_unless(!buf1.has(buf1.begin(), S + ins_cnt + 1));
		fail_unless(buf1.begin().has(S + ins_cnt));
		fail_unless(!buf1.begin().has(S + ins_cnt + 1));

		/* It is ok to move to itself. */
		buf1 = std::move(buf1);
		fail_unless(itr0 == buf1.begin());
		fail_unless(itr1 == itr_at(buf1, 1 + ins_cnt));
		fail_unless(itr2 == itr_at(buf1, S / 2 + ins_cnt));
		fail_unless(itr3 == itr_at(buf1, S - 1 + ins_cnt));
		fail_unless(*itr1 == expect1);
		fail_unless(*itr2 == expect2);
		fail_unless(*itr3 == expect3);
		fail_unless(buf1.has(S + ins_cnt));
		fail_unless(!buf1.has(S + ins_cnt + 1));
		fail_unless(buf1.has(buf1.begin(), S + ins_cnt));
		fail_unless(!buf1.has(buf1.begin(), S + ins_cnt + 1));
		fail_unless(buf1.begin().has(S + ins_cnt));
		fail_unless(!buf1.begin().has(S + ins_cnt + 1));
		buf1.flush();
		fail_unless(buf1.has(S + ins_cnt));
		fail_unless(!buf1.has(S + ins_cnt + 1));
		fail_unless(buf1.has(buf1.begin(), S + ins_cnt));
		fail_unless(!buf1.has(buf1.begin(), S + ins_cnt + 1));
		fail_unless(buf1.begin().has(S + ins_cnt));
		fail_unless(!buf1.begin().has(S + ins_cnt + 1));

		/*
		 * If we insert some data to the beginning of buffer, the
		 * iterators must be moved in order to point to the same data,
		 * except iterator to the beginning, which remains the same.
		 */
		buf1.insert(buf1.begin(), 1);
		++ins_cnt;
		fail_unless(itr0 == buf1.begin());
		auto x = buf1.begin();
		auto y = x + 1;
		fail_unless(itr1 == itr_at(buf1, 1 + ins_cnt));
		fail_unless(itr2 == itr_at(buf1, S / 2 + ins_cnt));
		fail_unless(itr3 == itr_at(buf1, S - 1 + ins_cnt));
		fail_unless(*itr1 == expect1);
		fail_unless(*itr2 == expect2);
		fail_unless(*itr3 == expect3);
		fail_unless(buf1.has(S + ins_cnt));
		fail_unless(!buf1.has(S + ins_cnt + 1));
		fail_unless(buf1.has(buf1.begin(), S + ins_cnt));
		fail_unless(!buf1.has(buf1.begin(), S + ins_cnt + 1));
		fail_unless(buf1.begin().has(S + ins_cnt));
		fail_unless(!buf1.begin().has(S + ins_cnt + 1));
		buf1.flush();
		fail_unless(buf1.has(S + ins_cnt));
		fail_unless(!buf1.has(S + ins_cnt + 1));
		fail_unless(buf1.has(buf1.begin(), S + ins_cnt));
		fail_unless(!buf1.has(buf1.begin(), S + ins_cnt + 1));
		fail_unless(buf1.begin().has(S + ins_cnt));
		fail_unless(!buf1.begin().has(S + ins_cnt + 1));

		/*
		 * Move constructor to new buffer.
		 * Iterators now belongs to it.
		 */
		tnt::Buffer<N> buf2(std::move(buf1));
		fail_unless(itr0 == buf2.begin());
		fail_unless(itr1 == itr_at(buf2, 1 + ins_cnt));
		fail_unless(itr2 == itr_at(buf2, S / 2 + ins_cnt));
		fail_unless(itr3 == itr_at(buf2, S - 1 + ins_cnt));
		fail_unless(*itr1 == expect1);
		fail_unless(*itr2 == expect2);
		fail_unless(*itr3 == expect3);
		fail_unless(buf2.has(S + ins_cnt));
		fail_unless(!buf2.has(S + ins_cnt + 1));
		fail_unless(buf2.has(buf2.begin(), S + ins_cnt));
		fail_unless(!buf2.has(buf2.begin(), S + ins_cnt + 1));
		fail_unless(buf2.begin().has(S + ins_cnt));
		fail_unless(!buf2.begin().has(S + ins_cnt + 1));
		buf2.flush();
		fail_unless(buf2.has(S + ins_cnt));
		fail_unless(!buf2.has(S + ins_cnt + 1));
		fail_unless(buf2.has(buf2.begin(), S + ins_cnt));
		fail_unless(!buf2.has(buf2.begin(), S + ins_cnt + 1));
		fail_unless(buf2.begin().has(S + ins_cnt));
		fail_unless(!buf2.begin().has(S + ins_cnt + 1));

		/*
		 * If we insert some data to the beginning of buffer, the
		 * iterators must be moved in order to point to the same data.
		 */
		buf2.insert(buf2.begin(), 1);
		++ins_cnt;
		fail_unless(itr0 == buf2.begin());
		fail_unless(itr1 == itr_at(buf2, 1 + ins_cnt));
		fail_unless(itr2 == itr_at(buf2, S / 2 + ins_cnt));
		fail_unless(itr3 == itr_at(buf2, S - 1 + ins_cnt));
		fail_unless(*itr1 == expect1);
		fail_unless(*itr2 == expect2);
		fail_unless(*itr3 == expect3);
		fail_unless(buf2.has(S + ins_cnt));
		fail_unless(!buf2.has(S + ins_cnt + 1));
		fail_unless(buf2.has(buf2.begin(), S + ins_cnt));
		fail_unless(!buf2.has(buf2.begin(), S + ins_cnt + 1));
		fail_unless(buf2.begin().has(S + ins_cnt));
		fail_unless(!buf2.begin().has(S + ins_cnt + 1));
		buf2.flush();
		fail_unless(buf2.has(S + ins_cnt));
		fail_unless(!buf2.has(S + ins_cnt + 1));
		fail_unless(buf2.has(buf2.begin(), S + ins_cnt));
		fail_unless(!buf2.has(buf2.begin(), S + ins_cnt + 1));
		fail_unless(buf2.begin().has(S + ins_cnt));
		fail_unless(!buf2.begin().has(S + ins_cnt + 1));

		{
			/*
			 * Move assignment to new buffer.
			 * Iterators now belongs to it.
			 */
			tnt::Buffer<N> buf3;
			fillBuffer(buf3, 1);
			buf3 = std::move(buf2);

			fail_unless(itr0 == buf3.begin());
			fail_unless(itr1 == itr_at(buf3, 1 + ins_cnt));
			fail_unless(itr2 == itr_at(buf3, S / 2 + ins_cnt));
			fail_unless(itr3 == itr_at(buf3, S - 1 + ins_cnt));
			fail_unless(*itr1 == expect1);
			fail_unless(*itr2 == expect2);
			fail_unless(*itr3 == expect3);
			fail_unless(buf3.has(S + ins_cnt));
			fail_unless(!buf3.has(S + ins_cnt + 1));
			fail_unless(buf3.has(buf3.begin(), S + ins_cnt));
			fail_unless(!buf3.has(buf3.begin(), S + ins_cnt + 1));
			fail_unless(buf3.begin().has(S + ins_cnt));
			fail_unless(!buf3.begin().has(S + ins_cnt + 1));
			buf3.flush();
			fail_unless(buf3.has(S + ins_cnt));
			fail_unless(!buf3.has(S + ins_cnt + 1));
			fail_unless(buf3.has(buf3.begin(), S + ins_cnt));
			fail_unless(!buf3.has(buf3.begin(), S + ins_cnt + 1));
			fail_unless(buf3.begin().has(S + ins_cnt));
			fail_unless(!buf3.begin().has(S + ins_cnt + 1));

			/*
			 * If we insert some data to the beginning of buffer,
			 * the iterators must be moved in order to point to the
			 * same data.
			 */
			buf3.insert(buf3.begin(), 1);
			++ins_cnt;
			fail_unless(itr0 == buf3.begin());
			fail_unless(itr1 == itr_at(buf3, 1 + ins_cnt));
			fail_unless(itr2 == itr_at(buf3, S / 2 + ins_cnt));
			fail_unless(itr3 == itr_at(buf3, S - 1 + ins_cnt));
			fail_unless(*itr1 == expect1);
			fail_unless(*itr2 == expect2);
			fail_unless(*itr3 == expect3);
			fail_unless(buf3.has(S + ins_cnt));
			fail_unless(!buf3.has(S + ins_cnt + 1));
			fail_unless(buf3.has(buf3.begin(), S + ins_cnt));
			fail_unless(!buf3.has(buf3.begin(), S + ins_cnt + 1));
			fail_unless(buf3.begin().has(S + ins_cnt));
			fail_unless(!buf3.begin().has(S + ins_cnt + 1));
			buf3.flush();
			fail_unless(buf3.has(S + ins_cnt));
			fail_unless(!buf3.has(S + ins_cnt + 1));
			fail_unless(buf3.has(buf3.begin(), S + ins_cnt));
			fail_unless(!buf3.has(buf3.begin(), S + ins_cnt + 1));
			fail_unless(buf3.begin().has(S + ins_cnt));
			fail_unless(!buf3.begin().has(S + ins_cnt + 1));
		}
	}
}

int main()
{
	buffer_basic<SMALL_BLOCK_SZ>();
	buffer_basic<LARGE_BLOCK_SZ>();
	buffer_add_read<SMALL_BLOCK_SZ>();
	buffer_add_read<LARGE_BLOCK_SZ>();
	buffer_iterator<SMALL_BLOCK_SZ>();
	buffer_iterator<LARGE_BLOCK_SZ>();
	buffer_insert<SMALL_BLOCK_SZ>();
	buffer_insert<LARGE_BLOCK_SZ>();
	buffer_release<SMALL_BLOCK_SZ>();
	buffer_release<LARGE_BLOCK_SZ>();
	buffer_out<SMALL_BLOCK_SZ>();
	buffer_out<LARGE_BLOCK_SZ>();
	buffer_iterator_get<SMALL_BLOCK_SZ>();
	buffer_iterator_get<LARGE_BLOCK_SZ>();
	buffer_move<SMALL_BLOCK_SZ>();
	buffer_move<LARGE_BLOCK_SZ>();

}
