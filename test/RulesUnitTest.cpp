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

#include "../src/mpp/Rules.hpp"

#include <array>
#include <string>

#include "Utils/Helpers.hpp"

namespace test {
using fis_t = std::make_index_sequence<mpp::compact::MP_END>;

template <class R, class _ = void>
struct has_simplex : std::false_type {};

template <class R>
struct has_simplex<R, std::void_t<decltype(R::simplex_tag)>> : std::true_type{};

template <class R, class _ = void>
struct has_complex : std::false_type {};

template <class R>
struct has_complex<R, std::void_t<decltype(R::complex_tag)>> : std::true_type{};

template <class RULE, uint8_t TAG>
constexpr bool rule_has_simplex_tag_h() {
	if constexpr (RULE::has_simplex) {
		using simplex_value_t = typename RULE::simplex_value_t;
		constexpr auto t = static_cast<simplex_value_t>(TAG);
		constexpr auto r = mpp::rule_simplex_tag_range_v<RULE>;
		return t >= r.first && t <= r.last;
	} else {
		return false;
	}
}

template <class RULE, uint8_t TAG>
constexpr bool rule_has_simplex_tag_v = rule_has_simplex_tag_h<RULE, TAG>();

template <class RULE, uint8_t TAG>
constexpr bool rule_has_complex_tag_h() {
	if constexpr (RULE::has_complex) {
		constexpr auto r = mpp::rule_complex_tag_range_v<RULE>;
		return TAG >= r.first && TAG <= r.last;
	} else {
		return false;
	}
}

template <class RULE, uint8_t TAG>
constexpr bool rule_has_complex_tag_v = rule_has_complex_tag_h<RULE, TAG>();

template <class RULE, uint8_t TAG>
constexpr bool rule_has_tag_v =
	rule_has_simplex_tag_v<RULE, TAG> || rule_has_complex_tag_v<RULE, TAG>;

template <uint8_t TAG, size_t FAMILY, size_t ...MORE>
constexpr auto rule_by_tag_h(std::index_sequence<FAMILY, MORE...>)
{
	constexpr mpp::compact::Family family{FAMILY};
	using rule_t = mpp::rule_by_family_t<family>;
	if constexpr (rule_has_tag_v<rule_t, TAG>) {
		return rule_t{};
	} else {
		static_assert(sizeof...(MORE) > 0);
		return rule_by_tag_h<TAG>(std::index_sequence<MORE...>{});
	}
}

template <uint8_t TAG>
using rule_by_tag_t = decltype(rule_by_tag_h<TAG>(test::fis_t{}));

} // namespace test

template <mpp::compact::Family FAMILY>
void
check_family_rule()
{
	using Rule_t = mpp::rule_by_family_t<FAMILY>;
	static_assert(Rule_t::family == FAMILY);
	static_assert(Rule_t::has_simplex == test::has_simplex<Rule_t>::value);
	static_assert(Rule_t::has_complex == test::has_complex<Rule_t>::value);
	if constexpr (Rule_t::has_simplex)
		static_assert(Rule_t::simplex_value_range.first == 0 ||
			      (Rule_t::simplex_value_range.first < 0 &&
			       FAMILY == mpp::compact::MP_INT));
}

template <size_t ...FAMILY>
void
check_family_rule(std::index_sequence<FAMILY...>)
{
	(check_family_rule<static_cast<mpp::compact::Family>(FAMILY)>(), ...);
}

void
test_basic()
{
	TEST_INIT(0);

	check_family_rule(test::fis_t{});

	using namespace mpp;

	// Some selective checks.
	static_assert(std::is_same_v<std::tuple<bool>, BoolRule::types>);
	static_assert(std::is_same_v<std::tuple<uint64_t, int64_t>, IntRule::types>);
	static_assert(NilRule::family == compact::MP_NIL);
	static_assert(BinRule::family == compact::MP_BIN);
	static_assert(IgnrRule::has_data == false);
	static_assert(StrRule::has_data == true);
	static_assert(BinRule::has_data == true);
	static_assert(ExtRule::has_data == true);
	static_assert(FltRule::has_ext == false);
	static_assert(ExtRule::has_ext == true);
	static_assert(FltRule::has_children == false);
	static_assert(ArrRule::has_children == true);
	static_assert(MapRule::has_children == true);
	static_assert(IntRule::children_multiplier == 0);
	static_assert(ArrRule::children_multiplier == 1);
	static_assert(MapRule::children_multiplier == 2);

	// More selective checks.
	static_assert(NilRule::has_simplex);
	static_assert(!NilRule::has_complex);
	static_assert(StrRule::has_simplex);
	static_assert(StrRule::has_complex);
	static_assert(!BinRule::has_simplex);
	static_assert(BinRule::has_complex);

	// rule_complex_count_v
	static_assert(rule_complex_count_v<NilRule> == 0);
	static_assert(rule_complex_count_v<FltRule> == 2);
	static_assert(rule_complex_count_v<IntRule> == 8);

	// And more selective checks.
	static_assert(rule_simplex_tag_range_v<IntRule>.first == -32);
	static_assert(rule_simplex_tag_range_v<IntRule>.last == 127);
	static_assert(rule_simplex_tag_range_v<IntRule>.count == 160);
	static_assert(rule_complex_tag_range_v<IntRule>.first == 0xcc);
	static_assert(rule_complex_tag_range_v<IntRule>.last == 0xd3);
	static_assert(rule_complex_tag_range_v<IntRule>.count == 8);
	static_assert(rule_simplex_tag_range_v<ArrRule>.first == 0x90);
	static_assert(rule_simplex_tag_range_v<ArrRule>.last == 0x9f);
	static_assert(rule_simplex_tag_range_v<ArrRule>.count == 16);
	static_assert(rule_complex_tag_range_v<ArrRule>.first == 0xdc);
	static_assert(rule_complex_tag_range_v<ArrRule>.last == 0xdd);
	static_assert(rule_complex_tag_range_v<ArrRule>.count == 2);
	static_assert(rule_simplex_tag_range_v<ExtRule>.first == 0xd4);
	static_assert(rule_simplex_tag_range_v<ExtRule>.last == 0xd8);
	static_assert(rule_simplex_tag_range_v<ExtRule>.count == 5);
	static_assert(rule_complex_tag_range_v<ExtRule>.first == 0xc7);
	static_assert(rule_complex_tag_range_v<ExtRule>.last == 0xc9);
	static_assert(rule_complex_tag_range_v<ExtRule>.count == 3);

	static_assert(find_simplex_offset<IntRule>(0) == 0);
	static_assert(find_simplex_offset<IntRule>(5) == 5);
	static_assert(find_simplex_offset<IntRule>(127) == 127);
	static_assert(find_simplex_offset<IntRule>(128) == 160);
	static_assert(find_simplex_offset<IntRule>(127.) == 127);
	static_assert(find_simplex_offset<IntRule>(128.) == 160);
	static_assert(find_simplex_offset<IntRule>(1280) == 160);
	static_assert(find_simplex_offset<IntRule>(-1) == -1);
	static_assert(find_simplex_offset<IntRule>(-32) == -32);
	static_assert(find_simplex_offset<IntRule>(-33) == 160);
	static_assert(find_simplex_offset<NilRule>(nullptr) == 0);
	static_assert(find_simplex_offset<NilRule>(1) == 0);
	static_assert(find_simplex_offset<BoolRule>(false) == 0);
	static_assert(find_simplex_offset<BoolRule>(true) == 1);
	static_assert(find_simplex_offset<BoolRule>(3) == 1);
	static_assert(find_simplex_offset<MapRule>(0) == 0);
	static_assert(find_simplex_offset<MapRule>(15) == 15);
	static_assert(find_simplex_offset<MapRule>(16) == 16);
	static_assert(find_simplex_offset<MapRule>(17) == 16);
	static_assert(find_simplex_offset<ExtRule>(0) == 5);
	static_assert(find_simplex_offset<ExtRule>(1) == 0);
	static_assert(find_simplex_offset<ExtRule>(2) == 1);
	static_assert(find_simplex_offset<ExtRule>(4) == 2);
	static_assert(find_simplex_offset<ExtRule>(8) == 3);
	static_assert(find_simplex_offset<ExtRule>(16) == 4);
	static_assert(find_simplex_offset<ExtRule>(17) == 5);
	static_assert(find_simplex_offset<ExtRule>(3) == 5);
	static_assert(find_simplex_offset<ExtRule>(7) == 5);
	static_assert(find_simplex_offset<ExtRule>(9) == 5);

	static_assert(find_complex_offset<IntRule>(0) == 0);
	static_assert(find_complex_offset<IntRule>(255) == 0);
	static_assert(find_complex_offset<IntRule>(256) == 1);
	static_assert(find_complex_offset<IntRule>(65535) == 1);
	static_assert(find_complex_offset<IntRule>(65536) == 2);
	static_assert(find_complex_offset<IntRule>(0xFFFFFFFF) == 2);
	static_assert(find_complex_offset<IntRule>(0x100000000) == 3);
	static_assert(find_complex_offset<IntRule>(0x100000000000) == 3);
	static_assert(find_complex_offset<IntRule>(0u) == 0);
	static_assert(find_complex_offset<IntRule>(255u) == 0);
	static_assert(find_complex_offset<IntRule>(256u) == 1);
	static_assert(find_complex_offset<IntRule>(65535u) == 1);
	static_assert(find_complex_offset<IntRule>(65536u) == 2);
	static_assert(find_complex_offset<IntRule>(0xFFFFFFFFu) == 2);
	static_assert(find_complex_offset<IntRule>(0x100000000u) == 3);
	static_assert(find_complex_offset<IntRule>(0x100000000000u) == 3);
	static_assert(find_complex_offset<IntRule>(-1) == 4);
	static_assert(find_complex_offset<IntRule>(-128) == 4);
	static_assert(find_complex_offset<IntRule>(-129) == 5);
	static_assert(find_complex_offset<IntRule>(-32768) == 5);
	static_assert(find_complex_offset<IntRule>(-32769) == 6);
	static_assert(find_complex_offset<IntRule>(-0x80000000ll) == 6);
	static_assert(find_complex_offset<IntRule>(-0x80000001ll) == 7);
	static_assert(find_complex_offset<IntRule>(-0x80000001000ll) == 7);
	static_assert(find_complex_offset<FltRule>(1.f) == 0);
	static_assert(find_complex_offset<FltRule>(1) == 1);
	static_assert(find_complex_offset<FltRule>(1.) == 1);
	static_assert(find_complex_offset<ArrRule>(0) == 0);
	static_assert(find_complex_offset<ArrRule>(255) == 0);
	static_assert(find_complex_offset<ArrRule>(256) == 0);
	static_assert(find_complex_offset<ArrRule>(65535) == 0);
	static_assert(find_complex_offset<ArrRule>(65536) == 1);
	static_assert(find_complex_offset<ArrRule>(0xFFFFFFFF) == 1);
	// Wrong arg, find_complex_offset does not check it.
	static_assert(find_complex_offset<ArrRule>(0x100000000) == 1);

	static_assert(test::rule_has_tag_v<mpp::IntRule, 1>);
	static_assert(test::rule_has_simplex_tag_v<mpp::IntRule, 1>);
	static_assert(!test::rule_has_complex_tag_v<mpp::IntRule, 1>);
	static_assert(test::rule_has_tag_v<mpp::IntRule, 126>);
	static_assert(test::rule_has_simplex_tag_v<mpp::IntRule, 126>);
	static_assert(!test::rule_has_complex_tag_v<mpp::IntRule, 126>);
	static_assert(test::rule_has_tag_v<mpp::IntRule, 127>);
	static_assert(test::rule_has_simplex_tag_v<mpp::IntRule, 127>);
	static_assert(!test::rule_has_complex_tag_v<mpp::IntRule, 127>);
	static_assert(test::rule_has_tag_v<mpp::IntRule, 255>);
	static_assert(test::rule_has_simplex_tag_v<mpp::IntRule, 255>);
	static_assert(!test::rule_has_complex_tag_v<mpp::IntRule, 255>);
	static_assert(test::rule_has_tag_v<mpp::IntRule, 0xcc>);
	static_assert(!test::rule_has_simplex_tag_v<mpp::IntRule, 0xcc>);
	static_assert(test::rule_has_complex_tag_v<mpp::IntRule, 0xcc>);
	static_assert(test::rule_has_tag_v<mpp::IntRule, 0xd0>);
	static_assert(!test::rule_has_simplex_tag_v<mpp::IntRule, 0xd0>);
	static_assert(test::rule_has_complex_tag_v<mpp::IntRule, 0xd0>);
}

using Info = std::string;
using FullInfo = std::array<Info, 256>;

template <class T, size_t ...I>
constexpr std::array<size_t, sizeof...(I)> tuple_sizes_h(std::index_sequence<I...>)
{
	return {sizeof(std::tuple_element_t<I, T>)...};
}

template <class T>
constexpr std::array<size_t, std::tuple_size_v<T>> tuple_sizes =
	tuple_sizes_h<T>(std::make_index_sequence<std::tuple_size_v<T>>{});

template <mpp::compact::Family FAMILY>
void collectByType(FullInfo& infos)
{
	using Rule = mpp::rule_by_family_t<FAMILY>;
	if constexpr (Rule::has_simplex) {
		const auto& range = Rule::simplex_value_range;
		constexpr auto tag = Rule::simplex_tag;
		std::string set_info = std::string("fix");
		set_info += mpp::FamilyHumanName[FAMILY];
		using type = std::tuple_element_t<0, typename Rule::types>;
		if (std::is_integral_v<type>) {
			set_info += " ";
			if (range.first == range.last) {
				set_info += std::to_string(range.first);
			} else {
				set_info += std::to_string(range.first);
				set_info += "..";
				set_info += std::to_string(range.last);
			}
		}
		for (uint8_t j = range.first; (j - 1) != range.last; j++) {
			auto& info = infos[(uint8_t)(j + tag)];
			fail_unless(info.empty());
			info = set_info;
		}
	}
	if constexpr (Rule::has_complex) {
		using types = typename Rule::complex_types;
		for (size_t i = 0; i < std::tuple_size_v<types>; i++) {
			auto& info = infos[i + Rule::complex_tag];
			fail_unless(info.empty());
			info = mpp::FamilyHumanName[FAMILY];
			info += " ";
			info += std::to_string(tuple_sizes<types>[i] * 8);
		}
	}
}

template <size_t ...FAMILY>
FullInfo collectByType(std::index_sequence<FAMILY...>)
{
	FullInfo infos;
	(collectByType<static_cast<mpp::compact::Family>(FAMILY)>(infos), ...);
	return infos;
}

FullInfo collectByType()
{
	return collectByType(test::fis_t{});
}

template <uint8_t TAG>
void collectByTags(FullInfo& info)
{
	using rule_t = test::rule_by_tag_t<TAG>;
	static_assert(test::rule_has_tag_v<rule_t, TAG>);
	constexpr bool is_simplex = test::rule_has_simplex_tag_v<rule_t, TAG>;
	constexpr bool is_complex = test::rule_has_complex_tag_v<rule_t, TAG>;
	static_assert(is_simplex != is_complex);
	if constexpr (is_simplex) {
		const auto& range = rule_t::simplex_value_range;
		std::string set_info = std::string("fix");
		set_info += mpp::FamilyHumanName[rule_t::family];
		using type = std::tuple_element_t<0, typename rule_t::types>;
		if (std::is_integral_v<type>) {
			set_info += " ";
			set_info += std::to_string(range.first);
			if (range.first != range.last) {
				set_info += "..";
				set_info += std::to_string(range.last);
			}
		}
		info[TAG] = set_info;
	} else {
		using types = typename rule_t::complex_types;
		size_t type_idx = TAG - rule_t::complex_tag;
		info[TAG] = mpp::FamilyHumanName[rule_t::family];
		info[TAG] += " ";
		info[TAG] += std::to_string(tuple_sizes<types>[type_idx] * 8);
	}
}

template <size_t ...I>
FullInfo collectByTags(std::index_sequence<I...>)
{
	FullInfo res;
	(collectByTags<I>(res), ...);
	return res;
}

FullInfo collectByTags()
{
	return collectByTags(std::make_index_sequence<256>{});
}

void
test_collect_info()
{
	TEST_INIT(0);

	FullInfo info1 = collectByType();
	FullInfo info2 = collectByTags();
	fail_unless(info1 == info2);
	for (size_t i = 0; i < 256; i++) {
		//std::cout << info1[i] << std::endl;
		fail_if(info1[i].empty());
	}
}

int main()
{
	test_basic();
	test_collect_info();
}
