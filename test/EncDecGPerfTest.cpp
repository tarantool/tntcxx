/*
 * Copyright 2010-2023, Tarantool AUTHORS, please see AUTHORS file.
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

#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstring>
#include <random>

#include "../src/mpp/mpp.hpp"
#include "msgpuck.h"

enum random_variant {
	// Random numbers.
	RND,
	// Random numbers with worst distribution for codec.
	BAD,
};

enum codec_variant {
	// Reference msgpuck.
	PUC,
	// This mpp codec.
	MPP,
};

std::mt19937_64 gen{std::random_device{}()};
std::uniform_int_distribution<uint64_t> uint_chooser{0, 4};
std::uniform_int_distribution<uint64_t> uint_distr[5] {
	std::uniform_int_distribution<uint64_t>{0, 0x7F},
	std::uniform_int_distribution<uint64_t>{0x80, 0xFF},
	std::uniform_int_distribution<uint64_t>{0x100, 0xFFFF},
	std::uniform_int_distribution<uint64_t>{0x10000, 0xFFFFFFFF},
	std::uniform_int_distribution<uint64_t>{0x100000000ull},
};

template <random_variant R>
void generate(uint64_t &data)
{
	if constexpr (R == RND)
		data = gen();
	else
		data = uint_distr[uint_chooser(gen)](gen);
}

struct SimpleWData {
	const char* data;
	size_t size;
};

struct SimpleWriter {
	char *pos;

	void write(SimpleWData data)
	{
		memcpy(pos, data.data, data.size);
		pos += data.size;
	}
	template <class T>
	void write(T t)
	{
		memcpy(pos, &t, sizeof(t));
		pos += sizeof(t);
	}
	template <char... C>
	void write(mpp::CStr<C...>)
	{
		memcpy(pos, mpp::CStr<C...>::data, mpp::CStr<C...>::rnd_size);
		pos += mpp::CStr<C...>::size;
	}
};

struct SimpleRData {
	char* data;
	size_t size;
};

struct SimpleSkip {
	size_t size;
};

struct SimpleReader {
	const char *pos;

	void read(SimpleRData data)
	{
		memcpy(data.data, pos, data.size);
		pos += data.size;
	}
	void read(SimpleSkip data)
	{
		pos += data.size;
	}
	template <class T>
	void read(T &t)
	{
		memcpy(&t, pos, sizeof(t));
		pos += sizeof(t);
	}
	template <class T>
	T get()
	{
		T t;
		memcpy(&t, pos, sizeof(t));
		return t;
	}
};

template <codec_variant CODEC>
void encode(SimpleWriter &wr, uint64_t val)
{
	if constexpr (CODEC == PUC) {
		wr.pos = mp_encode_uint(wr.pos, val);
	} else {
		mpp::encode(wr, val);
	}
}

template <codec_variant CODEC>
void decode(SimpleReader &rd, uint64_t &val)
{
	if constexpr (CODEC == PUC) {
		val = mp_decode_uint(&rd.pos);
	} else {
		mpp::decode(rd, val);
	}
}

template <class T, size_t N, size_t M, random_variant R>
struct TestData {
	T orig_data[N];
	char orig_buffer[M];
	size_t orig_size;
	T dec_data[N];
	char enc_buffer[M];
	SimpleWriter wr;
	SimpleReader rd;
	void generate()
	{
		wr.pos = orig_buffer;
		for (auto &val : orig_data) {
			::generate<R>(val);
			::encode<PUC>(wr, val);
		}
		orig_size = wr.pos - orig_buffer;
		if (orig_size > M)
			abort();
		wr.pos = enc_buffer;
		rd.pos = orig_buffer;
		memset(enc_buffer, '#', sizeof(enc_buffer));
		memset(dec_data, '#', sizeof(dec_data));
	}
	void check_enc()
	{
		size_t got_size = wr.pos - enc_buffer;
		if (got_size != orig_size)
			abort();
		if (memcmp(orig_buffer, enc_buffer, orig_size) != 0)
			abort();
	}
	void check_dec()
	{
		for (size_t i = 0; i < N; i++) {
			if (orig_data[i] != dec_data[i])
				abort();
		}
	}
};

template <class T, codec_variant CODEC, random_variant R>
static void BM_write(benchmark::State& state)
{
	size_t total_count = 0;
	size_t total_size = 0;
	constexpr size_t N = 1024;
	TestData<T, N, sizeof(T) * N * 2, R> data;

	for (auto _ : state)
	{
		state.PauseTiming();
		data.generate();
		state.ResumeTiming();
		//benchmark::DoNotOptimize();

		const T *b = data.orig_data;
		const T *e = data.orig_data + N;
		for (; b != e; b++)
			encode<CODEC>(data.wr, *b);

		total_count += N;
		total_size += data.orig_size;

		state.PauseTiming();
		data.check_enc();
		state.ResumeTiming();
	}

	state.SetItemsProcessed(total_count);
	state.SetBytesProcessed(total_size);
}

template <class T, codec_variant CODEC, random_variant R>
static void BM_read(benchmark::State& state)
{
	size_t total_count = 0;
	size_t total_size = 0;
	constexpr size_t N = 1024;
	TestData<T, N, sizeof(T) * N * 2, R> data;

	for (auto _ : state)
	{
		state.PauseTiming();
		data.generate();
		state.ResumeTiming();
		//benchmark::DoNotOptimize();

		T *b = data.dec_data;
		T *e = data.dec_data + N;
		for (; b != e; b++)
			decode<CODEC>(data.rd, *b);

		total_count += N;
		total_size += data.orig_size;

		state.PauseTiming();
		data.check_dec();
		state.ResumeTiming();
	}

	state.SetItemsProcessed(total_count);
	state.SetBytesProcessed(total_size);
}

using u64 = uint64_t;

BENCHMARK_TEMPLATE(BM_write, u64, PUC, RND);
BENCHMARK_TEMPLATE(BM_write, u64, MPP, RND);
BENCHMARK_TEMPLATE(BM_write, u64, PUC, BAD);
BENCHMARK_TEMPLATE(BM_write, u64, MPP, BAD);
BENCHMARK_TEMPLATE(BM_read, u64, PUC, RND);
BENCHMARK_TEMPLATE(BM_read, u64, MPP, RND);
BENCHMARK_TEMPLATE(BM_read, u64, PUC, BAD);
BENCHMARK_TEMPLATE(BM_read, u64, MPP, BAD);

BENCHMARK_MAIN();
