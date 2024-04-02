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
#include <vector>
#include <optional>

#include "Utils/Helpers.hpp"
#include "Utils/RefVector.hpp"

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
		using T = decltype(x);
		if constexpr (sizeof(x) == 1) {
			return res;
		} else {
			for (size_t i = 0; i < sizeof(x); i++) {
				res = static_cast<T>(res << 8);
				res |= x & 0xFF;
				x = static_cast<T>(x >> 8);
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
		float x = 3.1415927f;
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
		  << (MP_INT) << " "
		  << (MP_BIN | MP_STR) << " "
		  << (MP_INT | MP_FLT) << "\n";

	std::cout << "(" << family_sequence<>{} << ") "
		  << "(" << family_sequence<compact::MP_NIL>{} << ") "
		  << "(" << family_sequence<compact::MP_INT,
					    compact::MP_FLT,
					    compact::MP_NIL>{} << ")\n";
}

enum E1 {
	ZERO1 = 0,
	FOR_BILLIONS = 4000000000u,
};

enum E2 {
	ZERO2 = 0,
	MUNUS_ONE_HUNDRED = -100,
};

template <class T>
struct OverflowGuard {
	T data = {};
	uint64_t guard = 0xdeadbeef;
	bool is_safe() {
		static_assert(sizeof(T) + sizeof(guard) == sizeof(*this));
		return guard == 0xdeadbeef;
	}
};

template <class T, class U>
bool are_equal(const T& t, const U& u)
{
	return std::size(t) == std::size(u) &&
	       std::equal(std::begin(t), std::end(t), std::begin(u));
}


void
test_basic()
{
	TEST_INIT(0);
	using Buf_t = tnt::Buffer<16 * 1024>;
	Buf_t buf;
	// Numbers.
	mpp::encode(buf, 0);
	mpp::encode(buf, 10);
	mpp::encode(buf, uint8_t(200), short(2000), 2000000, 4000000000u);
	mpp::encode(buf, FOR_BILLIONS, 20000000000ull, -1);
	mpp::encode(buf, MUNUS_ONE_HUNDRED, -100, -1000);
	mpp::encode(buf, 1.f, 2.);
	// Integral constants.
	mpp::encode(buf, std::integral_constant<int, 11>{});
	mpp::encode(buf, std::integral_constant<unsigned, 12>{});
	mpp::encode(buf, std::integral_constant<int, -13>{});
	mpp::encode(buf, std::integral_constant<int, 100500>{});
	mpp::encode(buf, std::integral_constant<unsigned, 100501>{});
	mpp::encode(buf, std::integral_constant<bool, false>{});
	mpp::encode(buf, std::integral_constant<bool, true>{});
	// Strings.
	mpp::encode(buf, "abc");
	const char *bbb = "defg";
	mpp::encode(buf, bbb);
	mpp::encode(buf, TNT_CON_STR("1234567890"));
	// Array.
	mpp::encode(buf, std::make_tuple());
	mpp::encode(buf, std::make_tuple(1., 2.f, "test", nullptr, false));
	// Map.
	// Integer keys.
	mpp::encode(buf, mpp::as_map(
		std::forward_as_tuple(10, true, 11, "val",
				      12, std::make_tuple(1, 2, 3))));
	// String keys.
	mpp::encode(buf, std::make_tuple(std::make_pair("key1", "val1"),
					 std::make_pair("key2", "val2")));
	// Mixed keys.
	mpp::encode(buf, mpp::as_map(std::make_tuple(1, "val1",
						     "key2", "val2")));
	// std::array
	std::array<int, 3> add_arr = {1, 2, 3};
	mpp::encode(buf, add_arr);
	// std::vector
	std::vector<unsigned int> add_vec = {4, 5, 6};
	mpp::encode(buf, add_vec);
	// std::set
	std::set<uint8_t> add_set = {7, 8};
	mpp::encode(buf, add_set);
	// std::map
	std::map<int, int> add_map;
	add_map[1] = 2;
	add_map[3] = 4;
	mpp::encode(buf, add_map);

	for (auto itr = buf.begin(); itr != buf.end(); ++itr) {
		uint8_t u = itr.get<uint8_t>();
		char c = static_cast<char>(u);
		const char *h = "0123456789ABCDEF";
		if (c >= 'a' && c <= 'z')
			std::cout << c;
		else
			std::cout << h[u / 16] << h[u % 16];
	}
	std::cout << std::endl;

	auto run = buf.begin<true>();
	// Numbers.
	uint64_t u0 = 999;
	int i10 = 0;
	fail_unless(mpp::decode(run, u0, i10));
	fail_if(u0 != 0);
	fail_if(i10 != 10);
	uint8_t u200 = 0;
	short i2k = 0;
	double d2M = 0;
	uint64_t u4G = 0;
	fail_unless(mpp::decode(run, u200, i2k, d2M, u4G));
	fail_if(u200 != 200);
	fail_if(i2k != 2000);
	fail_if(d2M != 2000000);
	fail_if(u4G != 4000000000u);
	E1 e1 = ZERO1;
	size_t i2M = 0;
	int8_t i_minus_one = 0;
	E2 e2 = ZERO2;
	int im100 = 0, im1000 = 0;
	fail_unless(mpp::decode(run, e1, i2M, i_minus_one, e2, im100, im1000));
	fail_if(e1 != FOR_BILLIONS);
	fail_if(i2M != 20000000000ull);
	fail_if(i_minus_one != -1);
	fail_if(e2 != MUNUS_ONE_HUNDRED);
	fail_if(im100 != -100);
	fail_if(im1000 != -1000);
	double d1 = 0, d2 = 0;
	fail_unless(mpp::decode(run, d1, d2));
	fail_if(d1 != 1.);
	fail_if(d2 != 2.);

	// Integral constants.
	int i11 = 0, i13 = 0, i100500 = 0;
	unsigned u12 = 0, u100501 = 0;
	fail_unless(mpp::decode(run, i11, u12, i13, i100500, u100501));
	fail_if(i11 != 11);
	fail_if(u12 != 12);
	fail_if(i13 != -13);
	fail_if(i100500 != 100500);
	fail_if(u100501 != 100501);
	bool bf = true, bt = false;
	fail_unless(mpp::decode(run, bf, bt));
	fail_if(bf != false);
	fail_if(bt != true);

	// Strings.
	std::string a;
	char b_data[10];
	size_t b_size = 0;
	auto b = tnt::make_ref_vector(b_data, b_size);
	fail_unless(mpp::decode(run, a, b));
	fail_if(a != "abc");
	fail_if(b.size() != 4);
	fail_if(memcmp(b_data, "defg", 4) != 0);

	auto run_save = run;
	{
		std::string c;
		fail_unless(mpp::decode(run, c));
		fail_if(c != "1234567890");
	}
	auto run_end = run;

	run = run_save;
	{
		char c[16] = {};
		fail_unless(mpp::decode(run, c));
		fail_if(std::string_view(c) != "1234567890");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::array<char, 16> c = {};
		fail_unless(mpp::decode(run, c));
		fail_if(std::string_view(c.data()) != "1234567890");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		OverflowGuard<char[8]> c = {};
		fail_unless(mpp::decode(run, c.data));
		fail_unless(c.is_safe());
		std::string_view res{c.data, sizeof(c.data)};
		fail_if(res != "12345678");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		OverflowGuard<std::array<char, 8>> c = {};
		fail_unless(mpp::decode(run, c.data));
		fail_unless(c.is_safe());
		std::string_view res{c.data.data(), c.data.size()};
		fail_if(res != "12345678");
		fail_if(run != run_end);
	}

	// Array.
	std::tuple<> arr1;
	fail_unless(mpp::decode(run, arr1));

	run_save = run;
	std::tuple<double, double, std::string, std::nullptr_t, bool> arr2;
	fail_unless(mpp::decode(run, arr2));
	fail_if(std::get<0>(arr2) != 1.);
	fail_if(std::get<1>(arr2) != 2.);
	fail_if(std::get<2>(arr2) != "test");
	fail_if(std::get<4>(arr2) != false);
	run_end = run;

	run = run_save;
	double a0 = 0;
	double a1 = 0;
	std::string a2 = "";
	std::nullptr_t a3;
	bool a4 = true;
	fail_unless(mpp::decode(run,
		mpp::as_arr(std::forward_as_tuple(a0, mpp::as_flt(a1),
						  a2, a3, a4))));
	fail_if(a0 != 1.);
	fail_if(a1 != 2.);
	fail_if(a2 != "test");
	fail_if(a4 != false);
	fail_if(run != run_end);

	run = run_save;
	a0 = 0;
	fail_unless(mpp::decode(run, mpp::as_arr(std::tie(a0))));
	fail_if(a0 != 1.);
	fail_if(run != run_end);

	run = run_save;
	fail_unless(mpp::decode(run, mpp::as_arr(std::tie())));
	fail_if(run != run_end);

	// Map.
	// Integer keys.
	run_save = run;
	std::vector<int> map_arr;
	std::vector<int> map_arr_expect{1, 2, 3};
	std::tuple<int, std::vector<int>&, int8_t, bool, size_t, std::string>
		map1(12, map_arr, 10, false, 11, "");
	fail_unless(mpp::decode(run, mpp::as_map(map1)));
	fail_if(std::get<0>(map1) != 12);
	fail_if(map_arr != map_arr_expect);
	fail_if(std::get<2>(map1) != 10);
	fail_if(std::get<3>(map1) != true);
	fail_if(std::get<4>(map1) != 11);
	fail_if(std::get<5>(map1) != "val");

	run = run_save;
	map_arr.clear();
	bool map_bool = false;
	auto ic10 = std::integral_constant<size_t, 10>{};
	fail_unless(mpp::decode(run,
		mpp::as_map(std::forward_as_tuple(mpp::as_int(ic10),
						  mpp::as_bool(map_bool),
						  mpp::as_int(12),
						  mpp::as_arr(map_arr)))));
	fail_if(map_bool != true);
	fail_if(map_arr != map_arr_expect);

	run = run_save;
	map_bool = false;
	map_arr.clear();
	fail_unless(mpp::decode(run, std::forward_as_tuple(
		std::make_pair(mpp::as_int(ic10), mpp::as_bool(map_bool)),
		std::make_pair(mpp::as_int(12), mpp::as_arr(map_arr)))));
	fail_if(map_bool != true);
	fail_if(map_arr != map_arr_expect);

	// String keys.
	run_save = run;
	{
		std::string k1 = "key1", k2 = "key2";
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(k1, v1, k2, v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "val2");
	}
	run_end = run;

	run = run_save;
	{
		std::string v1;
		char v2[16] = {};
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple("key1", v1, "key2", v2))));
		fail_if(v1 != "val1");
		fail_if(std::string_view(v2) != "val2");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string v1;
		std::array<char, 16> v2 = {};
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple("key1", v1, "key2", v2))));
		fail_if(v1 != "val1");
		fail_if(std::string_view(v2.data()) != "val2");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(TNT_CON_STR("key1"), v1,
					      TNT_CON_STR("key2"), v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "val2");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		const char *k1 = "key1";
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(k1, v1,
					      std::string_view("key2"), v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "val2");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(TNT_CON_STR("key1"), v1,
					      TNT_CON_STR("key666"), v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple("key1", v1,
					      "key666", v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(TNT_CON_STR("key1"), v1,
					      TNT_CON_STR("key666"), v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string v1;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple("key1", v1))));
		fail_if(v1 != "val1");
		fail_if(run != run_end);
	}

	// Mixed keys.
	run_save = run;
	{
		std::string k2 = "key2";
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(1, v1,
					      k2, v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "val2");
	}
	run_end = run;

	run = run_save;
	{
		std::string k2 = "key2";
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(666, v1,
					      k2, v2))));
		fail_if(v1 != "");
		fail_if(v2 != "val2");
		fail_if(run != run_end);
	}

	run = run_save;
	{
		std::string k2 = "key666";
		std::string v1, v2;
		fail_unless(mpp::decode(run, mpp::as_map(
			std::forward_as_tuple(1, v1,
					      k2, v2))));
		fail_if(v1 != "val1");
		fail_if(v2 != "");
		fail_if(run != run_end);
	}

	// std::array etc
	run_save = run;
	{
		std::array<int, 4> read_arr = {};
		std::array<int, 4> expected = {1, 2, 3, 0};
		fail_unless(mpp::decode(run, read_arr));
		fail_unless(are_equal(read_arr, expected));
	}

	run = run_save;
	{
		int read_arr[4] = {};
		int expected[4] = {1, 2, 3, 0};
		fail_unless(mpp::decode(run, read_arr));
		fail_unless(are_equal(read_arr, expected));
	}

	run = run_save;
	{
		int read_arr[4] = {};
		int expected[4] = {1, 2, 3, 0};
		size_t size = 0;
		auto vec = tnt::make_ref_vector(read_arr, size);
		fail_unless(mpp::decode(run, vec));
		fail_unless(are_equal(read_arr, expected));
	}

	run = run_save;
	{
		OverflowGuard<std::array<int, 2>> read_arr;
		std::array<int, 2> expected = {1, 2};
		fail_unless(mpp::decode(run, read_arr.data));
		fail_unless(read_arr.is_safe());
		fail_unless(are_equal(read_arr.data, expected));
	}

	run = run_save;
	{
		OverflowGuard<int[2]> read_arr;
		int expected[2] = {1, 2};
		fail_unless(mpp::decode(run, read_arr.data));
		fail_unless(read_arr.is_safe());
		fail_unless(are_equal(read_arr.data, expected));
	}

	run = run_save;
	{
		OverflowGuard<int[2]> read_arr;
		int expected[2] = {1, 2};
		size_t size = 0;
		auto vec = tnt::make_ref_vector(read_arr.data, size);
		fail_unless(mpp::decode(run, vec));
		fail_unless(read_arr.is_safe());
		fail_unless(are_equal(read_arr.data, expected));
	}

	// std::vector
	std::vector<unsigned int> read_vec;
	fail_unless(mpp::decode(run, read_vec));
	fail_unless(are_equal(read_vec, add_vec));

	// std::set
	std::set<uint8_t> read_set;
	fail_unless(mpp::decode(run, read_set));
	fail_unless(are_equal(read_set, add_set));

	// std::map
	std::map<int, int> read_map;
	fail_unless(mpp::decode(run, read_map));
	fail_unless(are_equal(read_map, add_map));
}

struct JustClass {
	int mpp;
	using mpp_dec = int;
};

struct WithMppMemberRule {
	int i;
	static constexpr auto mpp = &WithMppMemberRule::i;
};

struct WithEncMemberRule {
	int j;
	static constexpr auto mpp_enc = std::make_tuple(&WithEncMemberRule::j);
};

struct WithDecMemberRule {
	int k;
	static constexpr auto mpp_dec = std::make_tuple(&WithDecMemberRule::k);
};

struct WithMppSpecRule {
	int a;
};

struct WithEncSpecRule {
	int b;
};

struct WithDecSpecRule {
	int c;
};

template <>
struct mpp_rule<WithMppSpecRule> {
	static constexpr auto value = &WithMppSpecRule::a;
};

template <>
struct mpp_enc_rule<WithEncSpecRule> {
	static constexpr auto value = std::make_tuple(&WithEncSpecRule::b);
};

template <>
struct mpp_dec_rule<WithDecSpecRule> {
	static constexpr auto value = std::make_tuple(&WithDecSpecRule::c);
};

void test_class_rules()
{
	TEST_INIT(0);
	static_assert(!mpp::has_enc_rule_v<JustClass>);
	static_assert(!mpp::has_dec_rule_v<JustClass>);

	static_assert(mpp::has_enc_rule_v<WithMppMemberRule>);
	static_assert(mpp::has_dec_rule_v<WithMppMemberRule>);
	static_assert(mpp::has_enc_rule_v<WithEncMemberRule>);
	static_assert(!mpp::has_dec_rule_v<WithEncMemberRule>);
	static_assert(!mpp::has_enc_rule_v<WithDecMemberRule>);
	static_assert(mpp::has_dec_rule_v<WithDecMemberRule>);

	static_assert(mpp::has_enc_rule_v<WithMppSpecRule>);
	static_assert(mpp::has_dec_rule_v<WithMppSpecRule>);
	static_assert(mpp::has_enc_rule_v<WithEncSpecRule>);
	static_assert(!mpp::has_dec_rule_v<WithEncSpecRule>);
	static_assert(!mpp::has_enc_rule_v<WithDecSpecRule>);
	static_assert(mpp::has_dec_rule_v<WithDecSpecRule>);

	fail_unless(&mpp::get_enc_rule<WithMppMemberRule>() ==
		    &WithMppMemberRule::mpp);
	fail_unless(&mpp::get_dec_rule<WithMppMemberRule>() ==
		    &WithMppMemberRule::mpp);
	fail_unless(&mpp::get_enc_rule<WithEncMemberRule>() ==
		    &WithEncMemberRule::mpp_enc);
	fail_unless(&mpp::get_dec_rule<WithDecMemberRule>() ==
		    &WithDecMemberRule::mpp_dec);

	fail_unless(&mpp::get_enc_rule<WithMppSpecRule>() ==
		    &mpp_rule<WithMppSpecRule>::value);
	fail_unless(&mpp::get_dec_rule<WithMppSpecRule>() ==
		    &mpp_rule<WithMppSpecRule>::value);
	fail_unless(&mpp::get_enc_rule<WithEncSpecRule>() ==
		    &mpp_enc_rule<WithEncSpecRule>::value);
	fail_unless(&mpp::get_dec_rule<WithDecSpecRule>() ==
		    &mpp_dec_rule<WithDecSpecRule>::value);
}

struct IntegerWrapper {
	int i = 0;
	void gen()
	{
		i = 42;
	}

	bool operator==(const IntegerWrapper& that) const
	{
		return i == that.i;
	}

	bool operator<(const IntegerWrapper& that) const
	{
		return i < that.i;
	}

	static constexpr auto mpp = &IntegerWrapper::i;
};

struct Triplet {
	int a = 0;
	int b = 0;
	int c = 0;

	void gen(int i)
	{
		a = 3 * i + 1;
		b = 3 * i + 100500;
		c = 3 * i + 3;
	}

	bool operator==(const Triplet& that) const
	{
		return std::tie(a, b, c) == std::tie(that.a, that.b, that.c);
	}

	bool operator<(const Triplet& that) const
	{
		return std::tie(a, b, c) < std::tie(that.a, that.b, that.c);
	}
};

template <>
struct mpp_rule<Triplet> {
	static constexpr auto value = std::make_tuple(&Triplet::a,
						      &Triplet::b,
						      &Triplet::c);
};

struct Error {
	int code = 0;
	std::string descr;

	void gen(int i)
	{
		code = static_cast<int>(i + 1);
		descr = std::to_string(code);
	}

	bool operator==(const Error& that) const
	{
		return std::tie(code, descr) == std::tie(that.code, that.descr);
	}

	bool operator<(const Error& that) const
	{
		return std::tie(code, descr) < std::tie(that.code, that.descr);
	}

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(0, &Error::code),
		std::make_pair(1, &Error::descr));
};

struct Body {
	std::string str;
	IntegerWrapper num;
	std::vector<Triplet> triplets;
	std::vector<Error> errors;

	void gen()
	{
		str = "gen";
		num.gen();
		triplets.resize(2);
		for (size_t i = 0; i < triplets.size(); i++)
			triplets[i].gen(static_cast<int>(i));
		errors.resize(3);
		for (size_t i = 0; i < errors.size(); i++)
			errors[i].gen(static_cast<int>(i));
	}

	bool operator==(const Body& that) const
	{
		return std::tie(str, num, triplets, errors) ==
		       std::tie(that.str, that.num, that.triplets, that.errors);
	}

	bool operator<(const Body& that) const
	{
		return std::tie(str, num, triplets, errors) <
		       std::tie(that.str, that.num, that.triplets, that.errors);
	}

	static constexpr auto mpp = std::make_tuple(
		std::make_pair(0, &Body::str),
		std::make_pair(1, &Body::num),
		std::make_pair(2, &Body::triplets),
		std::make_pair(3, &Body::errors));
};

void
test_object_codec()
{
	TEST_INIT(0);

	using Buf_t = tnt::Buffer<16 * 1024>;
	Buf_t buf;

	Body wr, rd;
	std::set<Body> rds;
	wr.gen();

	mpp::encode(buf, wr);
	mpp::encode(buf, std::forward_as_tuple(wr));

	for (auto itr = buf.begin(); itr != buf.end(); ++itr) {
		uint8_t u = itr.get<uint8_t>();
		char c = static_cast<char>(u);
		const char *h = "0123456789ABCDEF";
		if (c >= 'a' && c <= 'z')
			std::cout << c;
		else
			std::cout << h[u / 16] << h[u % 16];
	}
	std::cout << std::endl;

	auto itr = buf.begin();
	mpp::decode(itr, rd);
	fail_unless(rd == wr);

	mpp::decode(itr, rds);
	fail_unless(rds.count(wr) > 0);

	fail_unless(itr == buf.end());
}

static void
test_optional()
{
	TEST_INIT(0);

	using Buf_t = tnt::Buffer<16 * 1024>;
	Buf_t buf;
	bool ok;

	TEST_CASE("number");
	mpp::encode(buf, std::optional<int>(100), std::optional<int>(),
		    std::optional<int>(42), std::nullopt);

	auto run = buf.begin<true>();
	std::optional<int> opt_num;
	ok = mpp::decode(run, opt_num);
	fail_unless(ok);
	fail_unless(opt_num.has_value());
	fail_unless(opt_num.value() == 100);

	ok = mpp::decode(run, opt_num);
	fail_unless(ok);
	fail_unless(!opt_num.has_value());

	ok = mpp::decode(run, opt_num);
	fail_unless(ok);
	fail_unless(opt_num.has_value());
	fail_unless(opt_num.value() == 42);

	ok = mpp::decode(run, opt_num);
	fail_unless(ok);
	fail_unless(!opt_num.has_value());
	fail_unless(opt_num == std::nullopt);

	buf.flush();

	TEST_CASE("containers with numbers");
	size_t null_idx = 4;
	mpp::encode(buf, std::make_optional(mpp::as_arr(
		std::forward_as_tuple(0, std::make_optional(1), 2, 3, std::optional<int>(), 5)
	)));
	mpp::encode(buf, nullptr);
	std::vector<std::optional<int>> opt_num_arr;
	std::set<std::optional<int>> opt_num_set;
	std::optional<std::vector<std::optional<int>>> opt_num_opt_arr;
	std::optional<std::set<std::optional<int>>> opt_num_opt_set;

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_num_arr);
	fail_unless(ok);
	fail_unless(opt_num_arr.size() == 6);
	for (size_t i = 0; i < 6; i++) {
		if (i == null_idx) {
			fail_unless(!opt_num_arr[i].has_value());
			continue;
		}
		int val = static_cast<int>(i);
		fail_unless(opt_num_arr[i].has_value());
		fail_unless(opt_num_arr[i].value() == val);
	}

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_num_set);
	fail_unless(ok);
	fail_unless(opt_num_set.size() == 6);
	for (size_t i = 0; i < 6; i++) {
		if (i == null_idx) {
			fail_unless(opt_num_set.count(i) == 0);
			fail_unless(opt_num_set.count(std::nullopt) == 1);
			continue;
		}
		fail_unless(opt_num_set.count(i) > 0);
	}

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_num_opt_arr);
	fail_unless(ok);
	fail_unless(opt_num_opt_arr.has_value());
	fail_unless(opt_num_opt_arr->size() == 6);
	for (size_t i = 0; i < 6; i++) {
		if (i == null_idx) {
			fail_unless(!opt_num_opt_arr.value()[i].has_value());
			continue;
		}
		int val = static_cast<int>(i);
		fail_unless(opt_num_opt_arr.value()[i].has_value());
		fail_unless(opt_num_opt_arr.value()[i].value() == val);
	}
	ok = mpp::decode(run, opt_num_opt_arr);
	fail_unless(ok);
	fail_unless(!opt_num_opt_arr.has_value());

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_num_opt_set);
	fail_unless(ok);
	fail_unless(opt_num_opt_set.has_value());
	fail_unless(opt_num_opt_set->size() == 6);
	for (size_t i = 0; i < 6; i++) {
		if (i == null_idx) {
			fail_unless(opt_num_opt_set->count(i) == 0);
			fail_unless(opt_num_opt_set->count(std::nullopt) == 1);
			continue;
		}
		fail_unless(opt_num_opt_set->count(i) > 0);
	}
	ok = mpp::decode(run, opt_num_opt_set);
	fail_unless(ok);
	fail_unless(!opt_num_opt_set.has_value());

	buf.flush();

	TEST_CASE("trivial object");
	mpp::encode(buf, std::optional<IntegerWrapper>(IntegerWrapper {64}));

	run = buf.begin<true>();
	std::optional<IntegerWrapper> opt_int_wrapper;
	ok = mpp::decode(run, opt_int_wrapper);
	fail_unless(ok);
	fail_unless(opt_int_wrapper.has_value());
	fail_unless(opt_int_wrapper.value().i == 64);

	buf.flush();

	TEST_CASE("objects");
	Body wr;
	wr.gen();
	mpp::encode(buf, std::optional<Body>(wr), std::optional<Body>());

	run = buf.begin<true>();
	std::optional<Body> rd;
	ok = mpp::decode(run, rd);
	fail_unless(ok);
	fail_unless(rd.has_value());
	fail_unless(rd.value() == wr);

	ok = mpp::decode(run, rd);
	fail_unless(ok);
	fail_unless(!rd.has_value());

	buf.flush();

	TEST_CASE("containers with objects");
	std::vector<Body> wrs;
	for (size_t i = 0; i < 3; i++) {
		wrs.emplace_back();
		wrs[i].gen();
		wrs[i].str += std::to_string(i);
	}
	null_idx = 1;
	mpp::encode(buf, mpp::as_arr(std::forward_as_tuple(wrs[0], nullptr, wrs[2])));
	mpp::encode(buf, nullptr);
	std::vector<std::optional<Body>> opt_body_arr;
	std::set<std::optional<Body>> opt_body_set;
	std::optional<std::vector<std::optional<Body>>> opt_body_opt_arr;
	std::optional<std::set<std::optional<Body>>> opt_body_opt_set;

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_body_arr);
	fail_unless(ok);
	fail_unless(opt_body_arr.size() == 3);
	for (size_t i = 0; i < 3; i++) {
		if (i == null_idx) {
			fail_unless(!opt_body_arr[i].has_value());
			continue;
		}
		fail_unless(opt_body_arr[i].has_value());
		fail_unless(opt_body_arr[i].value() == wrs[i]);
	}

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_body_set);
	fail_unless(ok);
	fail_unless(opt_body_set.size() == 3);
	fail_unless(opt_body_set.count(std::nullopt) == 1);
	for (size_t i = 0; i < 3; i++) {
		if (i == null_idx) {
			fail_unless(opt_body_set.count(wrs[i]) == 0);
			continue;
		}
		fail_unless(opt_body_set.count(wrs[i]) > 0);
	}

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_body_opt_arr);
	fail_unless(ok);
	fail_unless(opt_body_opt_arr.has_value());
	fail_unless(opt_body_opt_arr->size() == 3);
	for (size_t i = 0; i < 3; i++) {
		if (i == null_idx) {
			fail_unless(!opt_body_opt_arr.value()[i].has_value());
			continue;
		}
		fail_unless(opt_body_opt_arr.value()[i].has_value());
		fail_unless(opt_body_opt_arr.value()[i].value() == wrs[i]);
	}
	ok = mpp::decode(run, opt_body_opt_arr);
	fail_unless(ok);
	fail_unless(!opt_body_opt_arr.has_value());

	run = buf.begin<true>();
	ok = mpp::decode(run, opt_body_opt_set);
	fail_unless(ok);
	fail_unless(opt_body_opt_set.has_value());
	fail_unless(opt_body_opt_set->size() == 3);
	fail_unless(opt_body_opt_set->count(std::nullopt) == 1);
	for (size_t i = 0; i < 3; i++) {
		if (i == null_idx) {
			fail_unless(opt_body_opt_set->count(wrs[i]) == 0);
			continue;
		}
		fail_unless(opt_body_opt_set->count(wrs[i]) > 0);
	}
	ok = mpp::decode(run, opt_body_opt_set);
	fail_unless(ok);
	fail_unless(!opt_body_opt_set.has_value());

	buf.flush();
}

static void
test_raw()
{
	TEST_INIT(0);
	using Buf_t = tnt::Buffer<16 * 1024>;
	Buf_t buf;
	std::string msg("Hello, test!");

	mpp::encode(buf, 10);

	mpp::encode(buf, msg);

	std::array<int, 3> add_arr = {1, 2, 3};
	mpp::encode(buf, add_arr);

	std::map<int, int> add_map = {{1, 2}, {3, 4}};
	mpp::encode(buf, add_map);

	using it_t = Buf_t::iterator_common<true>;
	it_t run = buf.begin<true>();
	
	std::pair<it_t, it_t> to_wrap;
	auto raw_decoders = std::make_tuple(
		std::pair<it_t, it_t>(),
		std::optional<std::pair<it_t, it_t>>(),
		mpp::as_raw(to_wrap)
	);

	auto check_each_raw_decoder = [&](auto& begin, auto& end) {
		auto &dec0 = std::get<0>(raw_decoders);
		fail_if(dec0.first != begin);
		fail_if(dec0.second != end);
		auto &dec1 = std::get<1>(raw_decoders);
		fail_if(!dec1.has_value());
		fail_if(dec1->first != begin);
		fail_if(dec1->second != end);
		auto &dec2 = mpp::unwrap(std::get<2>(raw_decoders));
		fail_if(dec2.first != begin);
		fail_if(dec2.second != end);
	};
	
	auto check_raw_decoders = [&](auto& begin, auto& end) {
		std::apply([&](auto&... decs){(
			...,
			[&](auto& dec) {
				auto it = begin;
				bool ok = mpp::decode(it, dec);
				fail_if(!ok);
				fail_if(it != end);
			}(decs));
		}, raw_decoders);
		check_each_raw_decoder(begin, end);
	};

	TEST_CASE("decode num");
	auto begin = run;
	int num;
	mpp::decode(run, num);
	check_raw_decoders(begin, run);

	TEST_CASE("decode string");
	begin = run;
	std::string str;
	mpp::decode(run, str);
	fail_if(str != msg);
	check_raw_decoders(begin, run);

	auto svp = run;
	TEST_CASE("decode the whole array");
	begin = run;
	std::array<int, 3> arr;
	mpp::decode(run, arr);
	fail_if(arr != add_arr);
	check_raw_decoders(begin, run);
	const auto arr_end = run;
	TEST_CASE("decode the first element of array");
	run = svp;
	begin = run;	
	mpp::decode(run, mpp::as_arr(std::forward_as_tuple(num)));
	fail_if(num != arr[0]);
	fail_if(run != arr_end);
	std::apply([&](auto&... decs){(
		...,
		[&](auto& dec) {
			auto it = begin;
			bool ok = mpp::decode(it,
				mpp::as_arr(std::forward_as_tuple(dec)));
			fail_if(!ok);
			fail_if(it != run);
		}(decs));
	}, raw_decoders);
	// Array is small - its header occupies one byte.
	auto elem_begin = begin + 1;
	auto elem_end = elem_begin;
	mpp::decode(elem_end, num);
	check_each_raw_decoder(elem_begin, elem_end);		
	TEST_CASE("decode the array key by key");
	run = svp;
	// Array is small - its header occupies one byte.
	run.read({1});
	for (size_t i = 0; i < std::size(arr); i++) {
		int val = 0;
		begin = run;
		mpp::decode(run, val);
		fail_if(val != static_cast<int>(i) + 1);
		check_raw_decoders(begin, run);
	}

	TEST_CASE("decode the whole map");
	svp = run;
	begin = run;
	int v1 = 0, v3 = 0;
	mpp::decode(run, mpp::as_map(std::forward_as_tuple(1, v1, 3, v3)));
	fail_if(v1 != 2);
	fail_if(v3 != 4);
	check_raw_decoders(begin, run);
	const auto map_end = run;
	TEST_CASE("decode one value from map");
	run = svp;
	begin = run;	
	mpp::decode(run, mpp::as_map(std::forward_as_tuple(1, num)));
	fail_if(run != map_end);
	fail_if(num != 2);
	std::apply([&](auto&... decs){(
		...,
		[&](auto& dec) {
			auto it = begin;
			bool ok = mpp::decode(it,
				mpp::as_map(std::forward_as_tuple(1, dec)));
			fail_if(!ok);
			fail_if(it != run);
		}(decs));
	}, raw_decoders);
	// Map is small - its header occupies one byte.
	elem_begin = begin + 1;
	// Skip key.
	mpp::decode(elem_begin, num);
	elem_end = elem_begin;
	// Skip value.
	mpp::decode(elem_end, num);
	check_each_raw_decoder(elem_begin, elem_end);		
	TEST_CASE("decode the map key by key");
	run = svp;
	// Map is small - its header occupies one byte.
	run.read({1});
	for (size_t i = 0; i < std::size(add_map); i++) {
		begin = run;
		int key = 0;
		mpp::decode(run, key);
		fail_if(key != 1 && key != 3);
		check_raw_decoders(begin, run);

		begin = run;
		int value = 0;
		mpp::decode(run, value);
		fail_if(value - key != 1);
		check_raw_decoders(begin, run);
	}
}

void
test_variant()
{
	TEST_INIT(0);

	using Buf_t = tnt::Buffer<16 * 1024>;
	Buf_t buf;
	auto run = buf.begin<true>();

	using variant_t = std::variant<bool, int, std::string, std::nullptr_t,
		std::optional<double>, std::vector<int>, Body>;

	variant_t wr;
	variant_t rd;

	wr.emplace<0>(true);
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);

	wr.emplace<1>(42);
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);
	
	wr.emplace<2>("string variant");
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);

	wr.emplace<3>(nullptr);
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);

	wr.emplace<4>(64.0);
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);

	wr.emplace<5>({1, 2, 3, 4, 5, 6, 7, 8});
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);

	Body body;
	body.gen();
	wr.emplace<6>(body);
	mpp::encode(buf, wr);
	mpp::decode(run, rd);
	fail_unless(wr == rd);

	std::variant<int, std::monostate, std::string> monostate_wr;
	std::variant<int, std::monostate, std::string> monostate_rd;

	monostate_wr.emplace<1>();
	mpp::encode(buf, monostate_wr);
	mpp::decode(run, monostate_rd);
	fail_unless(monostate_wr == monostate_rd);
}

int main()
{
	test_under_ints();
	test_bswap();
	test_type_visual();
	test_basic();
	test_class_rules();
	test_object_codec();
	test_optional();
	test_raw();
	test_variant();
}
