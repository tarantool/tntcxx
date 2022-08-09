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
#include "../src/mpp/mpp.hpp"
#include "../src/Buffer/Buffer.hpp"

#include <set>
#include <map>

#include "Utils/Helpers.hpp"

// Test mpp::under_uint_t and mpp::under_int_t
void
test_under_ints()
{
	TEST_INIT(0);

	static_assert(std::is_unsigned_v<mpp::under_uint_t<int8_t>>);
	static_assert(std::is_unsigned_v<mpp::under_uint_t<int16_t>>);
	static_assert(std::is_unsigned_v<mpp::under_uint_t<int32_t>>);
	static_assert(std::is_unsigned_v<mpp::under_uint_t<int64_t>>);

	static_assert(std::is_signed_v<mpp::under_int_t<uint8_t>>);
	static_assert(std::is_signed_v<mpp::under_int_t<uint16_t>>);
	static_assert(std::is_signed_v<mpp::under_int_t<uint32_t>>);
	static_assert(std::is_signed_v<mpp::under_int_t<uint64_t>>);

	static_assert(sizeof(mpp::under_uint_t<int8_t>) == 1);
	static_assert(sizeof(mpp::under_uint_t<int16_t>) == 2);
	static_assert(sizeof(mpp::under_uint_t<int32_t>) == 4);
	static_assert(sizeof(mpp::under_uint_t<int64_t>) == 8);

	static_assert(sizeof(mpp::under_int_t<uint8_t>) == 1);
	static_assert(sizeof(mpp::under_int_t<uint16_t>) == 2);
	static_assert(sizeof(mpp::under_int_t<uint32_t>) == 4);
	static_assert(sizeof(mpp::under_int_t<uint64_t>) == 8);
}

// Test bswap
void
test_bswap()
{
	auto bswap_naive = [](auto x) -> decltype(x)
	{
		static_assert(std::is_integral_v<decltype(x)>);
		static_assert(std::is_unsigned_v<decltype(x)>);
		auto res = x;
		if constexpr (sizeof(x) == 1) {
			return res;
		} else {
			for (size_t i = 0; i < sizeof(x); i++) {
				res <<= 8;
				res |= x & 0xFF;
				x >>= 8;
			}
		}
		return res;
	};

	enum E { V = 0x12345678u };

	uint64_t full = 0x1234567890123456ull;
	{
		auto x = (uint8_t) full;
		fail_unless(bswap_naive(x) == mpp::bswap(x));
	}
	{
		auto x = (uint16_t) full;
		fail_unless(bswap_naive(x) == mpp::bswap(x));
	}
	{
		auto x = (uint32_t) full;
		fail_unless(bswap_naive(x) == mpp::bswap(x));
	}
	{
		auto x = (uint64_t) full;
		fail_unless(bswap_naive(x) == mpp::bswap(x));
	}

	{
		auto x = (int8_t) full;
		using under_t = uint8_t;
		static_assert(std::is_same_v<under_t, decltype(mpp::bswap(x))>);
		fail_unless(bswap_naive((under_t) x) == mpp::bswap(x));
	}
	{
		auto x = (int16_t) full;
		using under_t = uint16_t;
		static_assert(std::is_same_v<under_t, decltype(mpp::bswap(x))>);
		fail_unless(bswap_naive((under_t) x) == mpp::bswap(x));
	}
	{
		auto x = (int32_t) full;
		using under_t = uint32_t;
		static_assert(std::is_same_v<under_t, decltype(mpp::bswap(x))>);
		fail_unless(bswap_naive((under_t) x) == mpp::bswap(x));
	}
	{
		auto x = (int64_t) full;
		using under_t = uint64_t;
		static_assert(std::is_same_v<under_t, decltype(mpp::bswap(x))>);
		fail_unless(bswap_naive((under_t) x) == mpp::bswap(x));
	}
	{
		float x = 3.1415927;
		using under_t = uint32_t;
		static_assert(std::is_same_v<under_t, decltype(mpp::bswap(x))>);
		under_t y;
		memcpy(&y, &x, sizeof(x));
		fail_unless(bswap_naive(y) == mpp::bswap(x));
		fail_unless(x == mpp::bswap<float>(mpp::bswap(x)));
	}
	{
		double x = 3.1415927;
		using under_t = uint64_t;
		static_assert(std::is_same_v<under_t, decltype(mpp::bswap(x))>);
		under_t y;
		memcpy(&y, &x, sizeof(x));
		fail_unless(bswap_naive(y) == mpp::bswap(x));
		fail_unless(x == mpp::bswap<double>(mpp::bswap(x)));
	}
	{
		auto x = V;
		using under_t = mpp::under_uint_t<E>;
		under_t y;
		memcpy(&y, &x, sizeof(x));
		fail_unless(bswap_naive(y) == mpp::bswap(x));
		fail_unless(x == mpp::bswap<E>(mpp::bswap(x)));
	}
}

void
test_type_visual()
{
	TEST_INIT(0);
	using namespace mpp;
	std::cout << compact::MP_ARR << " "
		  << compact::MP_MAP << " "
		  << compact::MP_EXT << "\n";
	std::cout << MP_NIL << " "
		  << MP_BOOL << " "
		  << (MP_UINT | MP_INT) << " "
		  << (MP_BIN  | MP_STR) << " "
		  << (MP_UINT | MP_INT | MP_DBL | MP_FLT) << "\n";
}

struct TestArrStruct {
	size_t parsed_arr_size;
	double dbl;
	float flt;
	char str[12];
	std::nullptr_t nil;
	bool b;
};


struct ArrValueReader : mpp::DefaultErrorHandler {
	using Buffer_t = tnt::Buffer<16 * 1024>;
	using BufferIterator_t = typename Buffer_t::iterator;
	explicit ArrValueReader(TestArrStruct& a) : arr(a) {}
	static constexpr mpp::Family VALID_TYPES = mpp::MP_DBL | mpp::MP_FLT |
		mpp::MP_STR | mpp::MP_NIL | mpp::MP_BOOL;
	template <class T>
	void Value(const BufferIterator_t&, mpp::compact::Family, T v)
	{
		using A = TestArrStruct;
		static constexpr std::tuple map(&A::dbl, &A::flt, &A::nil, &A::b);
		auto ptr = std::get<std::decay_t<T> A::*>(map);
		arr.*ptr = v;
	}
	void Value(BufferIterator_t& itr, mpp::compact::Family, mpp::StrValue v)
	{
		BufferIterator_t tmp = itr;
		tmp += v.offset;
		char *dst = arr.str;
		while (v.size) {
			*dst++ = *tmp;
			++tmp;
			--v.size;
		}
		*dst = 0;
	}

	BufferIterator_t* StoreEndIterator() { return nullptr; }
	TestArrStruct& arr;
};

struct ArrReader : mpp::SimpleReaderBase<tnt::Buffer<16 * 1024>, mpp::MP_ARR> {
	using Buffer_t = tnt::Buffer<16 * 1024>;
	using BufferIterator_t = typename Buffer_t::iterator;
	ArrReader(TestArrStruct& a, mpp::Dec<Buffer_t>& d) : arr(a), dec(d) {}
	void Value(const BufferIterator_t&, mpp::compact::Family, mpp::ArrValue v)
	{
		arr.parsed_arr_size = v.size;
		dec.SetReader(false, ArrValueReader{arr});
	}

	TestArrStruct& arr;
	mpp::Dec<Buffer_t>& dec;
};

namespace example {

using Buffer_t = tnt::Buffer<16 * 1024>;
using BufferIterator_t = typename Buffer_t::iterator;

struct TestMapStruct {
	bool boo;
	char str[12];
	size_t str_size;
	int arr[3];
	size_t arr_size;
};

struct MapKeyReader : mpp::SimpleReaderBase<Buffer_t, mpp::MP_UINT> {
	MapKeyReader(mpp::Dec<Buffer_t>& d, TestMapStruct& m) : dec(d), map(m) {}

	void Value(const BufferIterator_t&, mpp::compact::Family, uint64_t k)
	{
		using map_t = TestMapStruct;
		using Boo_t = mpp::SimpleReader<Buffer_t, mpp::MP_BOOL, bool>;
		using Str_t = mpp::SimpleStrReader<Buffer_t, sizeof(map_t{}.str)>;
		using Arr_t = mpp::SimpleArrReader
			<mpp::Dec<Buffer_t>,
			Buffer_t,
			sizeof(map_t{}.arr) / sizeof(map_t{}.arr[0]),
			mpp::MP_UINT,
			int
			>;
		switch (k) {
		case 10:
			dec.SetReader(true, Boo_t{map.boo});
			break;
		case 11:
			dec.SetReader(true, Str_t{map.str, map.str_size});
			break;
		case 12:
			dec.SetReader(true, Arr_t{dec, map.arr, map.arr_size});
			break;
		default:
			dec.AbortAndSkipRead();
		}
	}

	mpp::Dec<Buffer_t>& dec;
	TestMapStruct& map;
};


struct MapReader : mpp::SimpleReaderBase<tnt::Buffer<16 * 1024>, mpp::MP_MAP> {
	MapReader(mpp::Dec<Buffer_t>& d, TestMapStruct& m) : dec(d), map(m) {}

	void Value(const BufferIterator_t&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, MapKeyReader{dec, map});
	}

	mpp::Dec<Buffer_t>& dec;
	TestMapStruct& map;
};

} // namespace example {

template <class T>
struct IntVectorValueReader : mpp::DefaultErrorHandler {
	using Buffer_t = tnt::Buffer<16 * 1024>;
	using BufferIterator_t = typename Buffer_t::iterator;
	explicit IntVectorValueReader(std::vector<T>& v) : vec(v) {}
	static constexpr mpp::Family VALID_TYPES = std::is_signed_v<T> ?
	        mpp::MP_UINT | mpp::MP_INT : mpp::MP_UINT;
	template <class V>
	void Value(const BufferIterator_t&, mpp::compact::Family, V v)
	{
		vec.push_back(v);
	}

	BufferIterator_t* StoreEndIterator() { return nullptr; }
	std::vector<T>& vec;
};

template <class T>
struct IntVectorReader : mpp::SimpleReaderBase<tnt::Buffer<16 * 1024>, mpp::MP_ARR> {
	static_assert(std::is_integral_v<T>);
	static_assert(!std::is_same_v<T, bool>);
	using Buffer_t = tnt::Buffer<16 * 1024>;
	using BufferIterator_t = typename Buffer_t::iterator;
	IntVectorReader(std::vector<T>& v, mpp::Dec<Buffer_t>& d) : vec(v), dec(d) {}
	void Value(const BufferIterator_t&, mpp::compact::Family, mpp::ArrValue v)
	{
		vec.reserve(v.size);
		dec.SetReader(false, IntVectorValueReader{vec});
	}
	std::vector<T>& vec;
	mpp::Dec<Buffer_t>& dec;
};

template <class T, class U>
struct IntMapValueReader : mpp::DefaultErrorHandler {
	using Buffer_t = tnt::Buffer<16 * 1024>;
	using BufferIterator_t = typename Buffer_t::iterator;
	IntMapValueReader(std::map<T, U>& m, mpp::Dec<Buffer_t>& d) : map(m), dec(d) {}
	static constexpr mpp::Family VALID_TYPES = std::is_signed_v<T> ?
						   mpp::MP_UINT | mpp::MP_INT : mpp::MP_UINT;
	template <class V>
	void Value(const BufferIterator_t&, mpp::compact::Family, V v)
	{
		auto res = map.template emplace(v, 0);
		static constexpr mpp::Family VALID_TYPES = std::is_signed_v<U> ?
			mpp::MP_UINT | mpp::MP_INT : mpp::MP_UINT;
		using ValueReader_t = mpp::SimpleReader<Buffer_t, VALID_TYPES, U>;
		dec.SetReader(true, ValueReader_t{res.first->second});
	}

	BufferIterator_t* StoreEndIterator() { return nullptr; }
	std::map<T, U>& map;
	mpp::Dec<Buffer_t>& dec;
};

template <class T, class U>
struct IntMapReader : mpp::SimpleReaderBase<tnt::Buffer<16 * 1024>, mpp::MP_MAP> {
	static_assert(tnt::is_integer_v<T>);
	static_assert(tnt::is_integer_v<U>);
	using Buffer_t = tnt::Buffer<16 * 1024>;
	using BufferIterator_t = typename Buffer_t::iterator;
	IntMapReader(std::map<T, U>& m, mpp::Dec<Buffer_t>& d) : map(m), dec(d) {}
	void Value(const BufferIterator_t&, mpp::compact::Family, mpp::MapValue)
	{
		dec.SetReader(false, IntMapValueReader{map, dec});
	}
	std::map<T, U>& map;
	mpp::Dec<Buffer_t>& dec;
};

enum {
	MUNUS_ONE_HUNDRED = -100,
};


enum {
	FOR_BILLIONS = 4000000000u,
};

void
test_basic()
{
	TEST_INIT(0);
	using Buf_t = tnt::Buffer<16 * 1024>;
	Buf_t buf;
	tnt::mpp::encode(buf, 0);
	tnt::mpp::encode(buf, 10);
	tnt::mpp::encode(buf, uint8_t(200), short(2000), 2000000, 4000000000u);
	tnt::mpp::encode(buf, FOR_BILLIONS, 20000000000ull, -1);
	tnt::mpp::encode(buf, MUNUS_ONE_HUNDRED, -100, -1000);
	tnt::mpp::encode(buf, "aaa");
	const char* bbb = "bbb";
	tnt::mpp::encode(buf, bbb);
	// Add array.
	tnt::mpp::encode(buf, std::make_tuple());
	tnt::mpp::encode(buf, std::make_tuple(1., 2.f, "test", nullptr, false));
	// Add map.
	tnt::mpp::encode(buf, mpp::as_map(
		std::forward_as_tuple(10, true, 11, "val",
				      12, std::make_tuple(1, 2, 3))));
	// std::array
	std::array<int, 3> add_arr = {1, 2, 3};
	tnt::mpp::encode(buf, add_arr);
	// std::vector
	std::vector<unsigned int> add_vec = {4, 5, 6};
	tnt::mpp::encode(buf, add_vec);
	// std::set
	std::set<uint8_t> add_set = {7, 8};
	tnt::mpp::encode(buf, add_set);
	// std::map
	std::map<int, int> add_map;
	add_map[1] = 2;
	add_map[3] = 4;
	tnt::mpp::encode(buf, add_map);

	for (auto itr = buf.begin(); itr != buf.end(); ++itr) {
		char c = itr.get<uint8_t>();
		uint8_t u = c;
		const char *h = "0123456789ABCDEF";
		if (c >= 'a' && c <= 'z')
			std::cout << c;
		else
			std::cout << h[u / 16] << h[u % 16];
	}
	std::cout << std::endl;

	mpp::Dec<Buf_t> dec(buf);
	{
		int val = 15478;
		dec.SetReader(false, mpp::SimpleReader<Buf_t, mpp::MP_UINT, int>{val});
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(val != 0);
	}
	{
		int val = 15478;
		dec.SetReader(false, mpp::SimpleReader<Buf_t, mpp::MP_AINT, int>{val});
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(val != 10);
	}
	{
		unsigned short val = 15478;
		mpp::SimpleReader<Buf_t, mpp::MP_ANUM, unsigned short> r{val};
		dec.SetReader(false, r);
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(val != 200);
	}
	for (uint64_t exp : {2000ull, 2000000ull, 4000000000ull, 4000000000ull, 20000000000ull})
	{
		uint64_t val = 15478;
		dec.SetReader(false, mpp::SimpleReader<Buf_t, mpp::MP_UINT, uint64_t>{val});
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(val != exp);
	}
	for (int32_t exp : {-1, -100, -100, -1000})
	{
		int32_t val = 15478;
		dec.SetReader(false, mpp::SimpleReader<Buf_t, mpp::MP_AINT, int32_t>{val});
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(val != exp);
	}
	{
		constexpr size_t S = 16;
		char str[S];
		size_t size;
		dec.SetReader(false, mpp::SimpleStrReader<Buf_t, S - 1>{str, size});
		mpp::ReadResult_t res = dec.Read();
		str[size] = 0;
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(size != 3);
		fail_if(strcmp(str, "aaa") != 0);
	}
	{
		constexpr size_t S = 16;
		char str[S];
		size_t size;
		dec.SetReader(false, mpp::SimpleStrReader<Buf_t, S - 1>{str, size});
		mpp::ReadResult_t res = dec.Read();
		str[size] = 0;
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(size != 3);
		fail_if(strcmp(str, "bbb") != 0);
	}
	{
		TestArrStruct arr = {};
		dec.SetReader(false, ArrReader{arr, dec});
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(arr.parsed_arr_size != 0);

	}
	{
		TestArrStruct arr = {};
		dec.SetReader(false, ArrReader{arr, dec});
		mpp::ReadResult_t res = dec.Read();
		fail_if(res != mpp::READ_SUCCESS);
		fail_if(arr.parsed_arr_size != 5);
		fail_if(arr.dbl != 1.);
		fail_if(arr.flt != 2.f);
		fail_if(strcmp(arr.str, "test") != 0);
		fail_if(arr.nil != nullptr);
		fail_if(arr.b != false);

	}
	{
		using namespace example;
		TestMapStruct map = {};
		dec.SetReader(false, MapReader{dec, map});
		mpp::ReadResult_t res = dec.Read();
		fail_unless(res == mpp::READ_SUCCESS);
		fail_unless(map.boo == true);
		fail_unless(strcmp(map.str, "val") == 0);
		fail_unless(map.arr_size == 3);
		fail_unless(map.arr[0] == 1);
		fail_unless(map.arr[1] == 2);
		fail_unless(map.arr[2] == 3);
	}
	{
		std::vector<int> vec;
		dec.SetReader(false, IntVectorReader{vec, dec});
		mpp::ReadResult_t res = dec.Read();
		fail_unless(res == mpp::READ_SUCCESS);
		fail_unless(vec.size() == 3);
		fail_unless(vec[0] == 1);
		fail_unless(vec[1] == 2);
		fail_unless(vec[2] == 3);
	}
	{
		std::vector<short int> vec;
		dec.SetReader(false, IntVectorReader{vec, dec});
		mpp::ReadResult_t res = dec.Read();
		fail_unless(res == mpp::READ_SUCCESS);
		fail_unless(vec.size() == 3);
		fail_unless(vec[0] == 4);
		fail_unless(vec[1] == 5);
		fail_unless(vec[2] == 6);
	}
	{
		std::vector<uint8_t> vec;
		dec.SetReader(false, IntVectorReader{vec, dec});
		mpp::ReadResult_t res = dec.Read();
		fail_unless(res == mpp::READ_SUCCESS);
		fail_unless(vec.size() == 2);
		fail_unless(vec[0] == 7);
		fail_unless(vec[1] == 8);
	}
	{
		std::map<int, int> map;
		dec.SetReader(false, IntMapReader{map, dec});
		mpp::ReadResult_t res = dec.Read();
		fail_unless(res == mpp::READ_SUCCESS);
		fail_unless(map.size() == 2);
		fail_unless(map[1] == 2);
		fail_unless(map[3] == 4);
	}
}

int main()
{
	test_under_ints();
	test_bswap();
	test_type_visual();
	test_basic();
}
