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

void
test_basic()
{
	TEST_INIT(0);

	static_assert(std::is_same_v<bool, mpp::BoolRule::type>);
	static_assert(std::is_same_v<int64_t, mpp::IntRule::type>);
	static_assert(mpp::NilRule::family == mpp::compact::MP_NIL);
	static_assert(mpp::BinRule::family == mpp::compact::MP_BIN);
	static_assert(mpp::IgnrRule::has_data == false);
	static_assert(mpp::StrRule::has_data == true);
	static_assert(mpp::BinRule::has_data == true);
	static_assert(mpp::ExtRule::has_data == true);
	static_assert(mpp::FltRule::has_ext == false);
	static_assert(mpp::ExtRule::has_ext == true);
	static_assert(mpp::DblRule::has_children == false);
	static_assert(mpp::ArrRule::has_children == true);
	static_assert(mpp::MapRule::has_children == true);
	static_assert(mpp::UintRule::children_multiplier == 0);
	static_assert(mpp::ArrRule::children_multiplier == 1);
	static_assert(mpp::MapRule::children_multiplier == 2);
	static_assert(mpp::MapRule::is_negative == false);
	static_assert(mpp::IntRule::is_negative == true);
	static_assert(std::is_same_v<mpp::IntRule::positive_rule, mpp::UintRule>);
	static_assert(std::is_same_v<mpp::UintRule::positive_rule, void>);

	static_assert(mpp::has_simplex_v<mpp::NilRule>);
	static_assert(!mpp::has_complex_v<mpp::NilRule>);
	static_assert(mpp::has_simplex_v<mpp::StrRule>);
	static_assert(mpp::has_complex_v<mpp::StrRule>);
	static_assert(!mpp::has_simplex_v<mpp::BinRule>);
	static_assert(mpp::has_complex_v<mpp::BinRule>);

	using namespace mpp;
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_NIL >, NilRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_IGNR>, IgnrRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_BOOL>, BoolRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_UINT>, UintRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_INT >, IntRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_FLT >, FltRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_DBL >, DblRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_STR >, StrRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_BIN >, BinRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_ARR >, ArrRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_MAP >, MapRule>);
	static_assert(std::is_same_v<RuleByFamily_t<compact::MP_EXT >, ExtRule>);

	static_assert(SimplexRange<UintRule>::first == 0);
	static_assert(SimplexRange<UintRule>::second == 128);
	static_assert(SimplexRange<UintRule>::length == 128);
	static_assert(ComplexRange<UintRule>::first == 0xcc);
	static_assert(ComplexRange<UintRule>::second == 0xd0);
	static_assert(ComplexRange<UintRule>::length == 4);
	static_assert(SimplexRange<IntRule>::first == 0xe0);
	static_assert(SimplexRange<IntRule>::second == 256);
	static_assert(SimplexRange<IntRule>::length == 32);
	static_assert(ComplexRange<IntRule>::first == 0xd0);
	static_assert(ComplexRange<IntRule>::second == 0xd4);
	static_assert(ComplexRange<IntRule>::length == 4);
	static_assert(SimplexRange<ArrRule>::first == 0x90);
	static_assert(SimplexRange<ArrRule>::second == 0xa0);
	static_assert(SimplexRange<ArrRule>::length == 16);
	static_assert(ComplexRange<ArrRule>::first == 0xdc);
	static_assert(ComplexRange<ArrRule>::second == 0xde);
	static_assert(ComplexRange<ArrRule>::length == 2);
	static_assert(ComplexRange<IntRule>::length == 4);
	static_assert(SimplexRange<ExtRule>::first == 0xd4);
	static_assert(SimplexRange<ExtRule>::second == 0xd9);
	static_assert(SimplexRange<ExtRule>::length == 5);
	static_assert(ComplexRange<ExtRule>::first == 0xc7);
	static_assert(ComplexRange<ExtRule>::second == 0xca);
	static_assert(ComplexRange<ExtRule>::length == 3);

	static_assert(find_simplex_offset<UintRule>(0) == 0);
	static_assert(find_simplex_offset<UintRule>(5) == 5);
	static_assert(find_simplex_offset<UintRule>(127) == 127);
	static_assert(find_simplex_offset<UintRule>(128) == 128);
	static_assert(find_simplex_offset<UintRule>(1280) == 128);
	static_assert(find_simplex_offset<IntRule>(0) == 32);
	static_assert(find_simplex_offset<IntRule>(-1) == 31);
	static_assert(find_simplex_offset<IntRule>(-32) == 0);
	static_assert(find_simplex_offset<IntRule>(-33) == 32);
	static_assert(find_simplex_offset<IntRule>(1280) == 32);
	static_assert(find_simplex_offset<BoolRule>(false) == 0);
	static_assert(find_simplex_offset<BoolRule>(true) == 1);
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

	static_assert(find_complex_offset<UintRule>(0) == 0);
	static_assert(find_complex_offset<UintRule>(255) == 0);
	static_assert(find_complex_offset<UintRule>(256) == 1);
	static_assert(find_complex_offset<UintRule>(65535) == 1);
	static_assert(find_complex_offset<UintRule>(65536) == 2);
	static_assert(find_complex_offset<UintRule>(0xFFFFFFFF) == 2);
	static_assert(find_complex_offset<UintRule>(0x100000000) == 3);
	static_assert(find_complex_offset<UintRule>(0x100000000000) == 3);
	static_assert(find_complex_offset<UintRule>(0u) == 0);
	static_assert(find_complex_offset<UintRule>(255u) == 0);
	static_assert(find_complex_offset<UintRule>(256u) == 1);
	static_assert(find_complex_offset<UintRule>(65535u) == 1);
	static_assert(find_complex_offset<UintRule>(65536u) == 2);
	static_assert(find_complex_offset<UintRule>(0xFFFFFFFFu) == 2);
	static_assert(find_complex_offset<UintRule>(0x100000000u) == 3);
	static_assert(find_complex_offset<UintRule>(0x100000000000u) == 3);
	static_assert(find_complex_offset<IntRule>(-1) == 0);
	static_assert(find_complex_offset<IntRule>(-128) == 0);
	static_assert(find_complex_offset<IntRule>(-129) == 1);
	static_assert(find_complex_offset<IntRule>(-32768) == 1);
	static_assert(find_complex_offset<IntRule>(-32769) == 2);
	static_assert(find_complex_offset<IntRule>(-0x80000000ll) == 2);
	static_assert(find_complex_offset<IntRule>(-0x80000001ll) == 3);
	static_assert(find_complex_offset<IntRule>(-0x80000001000ll) == 3);
	static_assert(find_complex_offset<FltRule>(1) == 0);
	static_assert(find_complex_offset<ArrRule>(0) == 0);
	static_assert(find_complex_offset<ArrRule>(255) == 0);
	static_assert(find_complex_offset<ArrRule>(256) == 0);
	static_assert(find_complex_offset<ArrRule>(65535) == 0);
	static_assert(find_complex_offset<ArrRule>(65536) == 1);
	static_assert(find_complex_offset<ArrRule>(0xFFFFFFFF) == 1);
	// Wrong arg, find_complex_offset does not check it.
	static_assert(find_complex_offset<ArrRule>(0x100000000) == 1);

	static_assert(mpp::rule_has_tag_v<UintRule, 1>);
	static_assert(mpp::rule_has_simplex_tag_v<UintRule, 1>);
	static_assert(!mpp::rule_has_complex_tag_v<UintRule, 1>);
	static_assert(!mpp::rule_has_tag_v<UintRule, 255>);
	static_assert(!mpp::rule_has_simplex_tag_v<UintRule, 255>);
	static_assert(!mpp::rule_has_complex_tag_v<UintRule, 255>);
	static_assert(mpp::rule_has_tag_v<UintRule, 0xcc>);
	static_assert(!mpp::rule_has_simplex_tag_v<UintRule, 0xcc>);
	static_assert(mpp::rule_has_complex_tag_v<UintRule, 0xcc>);
	static_assert(!mpp::rule_has_tag_v<UintRule, 0xd0>);
	static_assert(!mpp::rule_has_simplex_tag_v<UintRule, 0xd0>);
	static_assert(!mpp::rule_has_complex_tag_v<UintRule, 0xd0>);
	static_assert(!mpp::rule_has_tag_v<IntRule, 1>);
	static_assert(!mpp::rule_has_simplex_tag_v<IntRule, 1>);
	static_assert(!mpp::rule_has_complex_tag_v<IntRule, 1>);
	static_assert(mpp::rule_has_tag_v<IntRule, 255>);
	static_assert(mpp::rule_has_simplex_tag_v<IntRule, 255>);
	static_assert(!mpp::rule_has_complex_tag_v<IntRule, 255>);
	static_assert(!mpp::rule_has_tag_v<IntRule, 0xcc>);
	static_assert(!mpp::rule_has_simplex_tag_v<IntRule, 0xcc>);
	static_assert(!mpp::rule_has_complex_tag_v<IntRule, 0xcc>);
	static_assert(mpp::rule_has_tag_v<IntRule, 0xd0>);
	static_assert(!mpp::rule_has_simplex_tag_v<IntRule, 0xd0>);
	static_assert(mpp::rule_has_complex_tag_v<IntRule, 0xd0>);
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
void collectType(FullInfo& infos)
{
	using Rule = mpp::RuleByFamily_t<FAMILY>;
	if constexpr (mpp::has_simplex_v<Rule>) {
		const auto& ranges = Rule::simplex_ranges;
		auto tag = Rule::simplex_tag;
		for (size_t i = 0; i < std::size(ranges); i++) {
			std::string set_info = std::string("fix");
			set_info += mpp::FamilyHumanName[FAMILY];
			if (std::is_integral_v<typename Rule::type>) {
				set_info += " ";
				if (ranges[i].first == ranges[i].second) {
					set_info += std::to_string(ranges[i].first);
				} else {
					set_info += std::to_string(ranges[i].first);
					set_info += "..";
					set_info += std::to_string(ranges[i].second);
				}
			}
			for (auto j = ranges[i].first; j <= ranges[i].second; j++) {
				auto& info = infos[(uint8_t)(tag++)];
				fail_unless(info.empty());
				info = set_info;
			}
		};
	}
	if constexpr (mpp::has_complex_v<Rule>) {
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

template <class TUPLE, size_t I, size_t ...J>
void collectByTypes(FullInfo& info, std::index_sequence<I, J...>)
{
	collectType<std::tuple_element_t<I, TUPLE>::value>(info);
	if constexpr (sizeof...(J) > 0)
		collectByTypes<TUPLE>(info, std::index_sequence<J...>{});
}

FullInfo collectByTypes()
{
	using namespace mpp;
	using families = std::tuple<
		std::integral_constant<compact::Family, compact::MP_NIL>,
		std::integral_constant<compact::Family, compact::MP_IGNR>,
		std::integral_constant<compact::Family, compact::MP_BOOL>,
		std::integral_constant<compact::Family, compact::MP_UINT>,
		std::integral_constant<compact::Family, compact::MP_INT>,
		std::integral_constant<compact::Family, compact::MP_FLT>,
		std::integral_constant<compact::Family, compact::MP_DBL>,
		std::integral_constant<compact::Family, compact::MP_STR>,
		std::integral_constant<compact::Family, compact::MP_BIN>,
		std::integral_constant<compact::Family, compact::MP_ARR>,
		std::integral_constant<compact::Family, compact::MP_MAP>,
		std::integral_constant<compact::Family, compact::MP_EXT>
	>;
	constexpr size_t S = std::tuple_size_v<families>;
	FullInfo res;
	collectByTypes<families>(res, std::make_index_sequence<S>{});
	return res;
}

template <uint8_t TAG>
void collectByTags(FullInfo& info)
{
	using rule_t = mpp::RuleByTag_t<TAG>;
	// Two lines below are built extremely slow, so disabled.
	//using alt_rule_t = std::tuple_element_t<TAG, mpp::AllTagRules_t>;
	//static_assert(std::is_same_v<rule_t, alt_rule_t>);
	static_assert(mpp::rule_has_tag<rule_t>(TAG));
	const bool r1 = mpp::rule_has_simplex_tag<rule_t>(TAG);
	const bool r2 = mpp::rule_has_complex_tag<rule_t>(TAG);
	static_assert(r1 != r2);
	if constexpr (r1) {
		const auto& ranges = rule_t::simplex_ranges;
		std::string set_info = std::string("fix");
		set_info += mpp::FamilyHumanName[rule_t::family];
		if (std::is_integral_v<typename rule_t::type>) {
			auto tag_offset = TAG - rule_t::simplex_tag;
			for (size_t i = 0; i < std::size(ranges); i++) {
				auto w = ranges[i].second - ranges[i].first + 1;
				if (tag_offset >= w) {
					tag_offset -= w;
					continue;
				}
				set_info += " ";
				set_info += std::to_string(ranges[i].first);
				if (ranges[i].first == ranges[i].second)
					break;
				set_info += "..";
				set_info += std::to_string(ranges[i].second);
				break;
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

	FullInfo info1 = collectByTypes();
	FullInfo info2 = collectByTags();
	fail_unless(info1 == info2);
}

int main()
{
	test_basic();
	test_collect_info();
}
