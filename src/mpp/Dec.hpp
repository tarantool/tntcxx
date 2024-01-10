#pragma once
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

#include <cassert>
#include <functional>
#include <cstdint>
#include <utility>

#include "ClassRule.hpp"
#include "Constants.hpp"
#include "Rules.hpp"
#include "Spec.hpp"

namespace mpp {

using std::integral_constant;
using tnt::CStr;
#ifndef TNT_DISABLE_STR_LITERAL
namespace literal = tnt::literal;
#endif

namespace decode_details {

constexpr auto is256 = tnt::make_iseq<256>{};

template <class T>
constexpr bool is_any_putable_v =
	tnt::is_emplacable_v<T> || tnt::is_back_emplacable_v<T> ||
	tnt::is_back_pushable_v<T> || tnt::is_insertable_v<T>;

/**
 * If it is true, the object of type T will not be decoded - raw data will
 * be saved to it.
 * 
 * Now it supports only a pair of iterators (probably, wrapped with
 * mpp::as_raw). The check implicilty implies that BUF is an iterator, not
 * buffer - it would be strange to pass a pair of buffer to decoder.
 */
template <class T, class BUF>
constexpr bool is_raw_decoded_v =
	is_wrapped_raw_v<T> || tnt::is_pairish_of_v<unwrap_t<T>, BUF, BUF>;

template <class T, class U>
void
put_to_putable(T& t, U&& u)
{
	if constexpr (tnt::is_back_emplacable_v<T>) {
		t.emplace_back(std::forward<U>(u));
	} else if constexpr (tnt::is_emplacable_v<T>) {
		t.emplace(std::forward<U>(u));
	} else  if constexpr (tnt::is_back_pushable_v<T>) {
		t.push_back(std::forward<U>(u));
	} else if constexpr (tnt::is_insertable_v<T>) {
		t.insert(std::forward<U>(u));
	} else {
		static_assert(tnt::always_false_v<T>);
	}
}

template <class T, size_t... I>
constexpr auto getFamiliesByRules(tnt::iseq<I...>)
{
	return family_sequence<tnt::tuple_element_t<I, T>::family...>{};
}

template <class T>
constexpr auto getFamiliesByRules()
{
	return getFamiliesByRules<T>(tnt::tuple_iseq<T>{});
}

template <class BUF, class T>
constexpr auto detectFamily();

template <class BUF, class T, size_t I = 0>
constexpr auto detectFamilyVariant()
{
	static_assert(tnt::is_variant_v<T>);
	if constexpr (I < std::variant_size_v<T>) {
		constexpr auto curFamily =
			detectFamily<BUF, std::variant_alternative_t<I, T>>();
		return curFamily + detectFamilyVariant<BUF, T, I + 1>();
	} else {
		return family_sequence<>{};
	}
}

template <class BUF, class T>
constexpr auto detectFamily()
{
	using U = unwrap_t<T>;
	static_assert(!std::is_const_v<U> || tnt::is_tuplish_v<U> ||
		      std::is_member_pointer_v<U>,
		      "Can't decode to constant type");
	if constexpr (std::is_member_pointer_v<U>) {
		return detectFamily<BUF, tnt::demember_t<U>>();
	} else if constexpr (is_raw_decoded_v<T, BUF>) {
		static_assert(!is_wrapped_family_v<T>);
		return getFamiliesByRules<all_rules_t>();
	} else if constexpr (is_wrapped_family_v<T>) {
		return family_sequence<T::family>{};
	} else if constexpr (has_dec_rule_v<U>) {
		return detectFamily<BUF, decltype(get_dec_rule<U>())>();
	} else if constexpr (tnt::is_optional_v<U>) {
		return family_sequence_populate<compact::MP_NIL>(
			detectFamily<BUF, tnt::value_type_t<U>>());
	} else if constexpr (tnt::is_variant_v<U>) {
		return detectFamilyVariant<BUF, U>();
	} else if constexpr (std::is_same_v<U, std::nullptr_t>) {
		return family_sequence<compact::MP_NIL>{};
	} else if constexpr (std::is_same_v<U, bool>) {
		return family_sequence<compact::MP_BOOL>{};
	} else if constexpr (tnt::is_integer_v<U>) {
		return family_sequence<compact::MP_INT>{};
	} else if constexpr (std::is_floating_point_v<U>) {
		return family_sequence<compact::MP_INT, compact::MP_FLT>{};
	} else if constexpr (tnt::is_contiguous_char_v<U>) {
		return family_sequence<compact::MP_STR>{};
	} else if constexpr (tnt::is_tuplish_v<U>) {
		if constexpr (tnt::tuple_size_v<U> == 0)
			return family_sequence<compact::MP_ARR,
					       compact::MP_MAP>{};
		else if constexpr (tnt::is_tuplish_of_pairish_v<U>)
			return family_sequence<compact::MP_MAP>{};
		else
			return family_sequence<compact::MP_ARR>{};
	} else if constexpr (is_any_putable_v<U> ||
			     tnt::is_contiguous_v<U>) {
		if constexpr (tnt::is_pairish_v<tnt::value_type_t<U>>)
			return family_sequence<compact::MP_MAP>{};
		else
			return family_sequence<compact::MP_ARR>{};
	} else {
		static_assert(tnt::always_false_v<U>,
			      "Failed to recognise type");
		return family_sequence<>{};
	}
}

template <compact::Family... FAMILY>
constexpr bool hasChildren(family_sequence<FAMILY...>)
{
	return (rule_by_family_t<FAMILY>::has_children || ...);
}

template <class BUF, class T>
constexpr auto hasChildren()
{
	if constexpr (std::is_same_v<void, T>)
		return false;
	else
		return hasChildren(detectFamily<BUF, T>());
}

enum path_item_type {
	PIT_BAD,
	PIT_STATIC_L0,
	PIT_STATIC,
	// Dynamic size below, see is_path_item_dynamic.
	PIT_STADYN,
	// Non-static size below, see is_path_item_static.
	PIT_DYN_POS,
	PIT_DYN_BACK,
	PIT_DYN_ADD,
	PIT_DYN_KEY,
	PIT_DYN_SKIP,
	PIT_OPTIONAL,
	PIT_VARIANT,
	PIT_RAW,
};

constexpr size_t PATH_ITEM_MULT = 1000000;

constexpr size_t path_item_new(path_item_type type,
			       size_t static_size = 0, size_t static_pos = 0)
{
	if (static_size >= PATH_ITEM_MULT)
		return path_item_new(PIT_BAD);
	return size_t(type) * PATH_ITEM_MULT * PATH_ITEM_MULT +
	       static_size * PATH_ITEM_MULT + static_pos;
}

constexpr size_t path_item_static_pos(size_t item)
{
	return item % PATH_ITEM_MULT;
}

constexpr size_t path_item_static_size(size_t item)
{
	return item / PATH_ITEM_MULT % PATH_ITEM_MULT;
}

constexpr enum path_item_type path_item_type(size_t item)
{
	return static_cast<enum path_item_type>(item / PATH_ITEM_MULT /
						PATH_ITEM_MULT);
}

constexpr bool is_path_item_static(size_t item)
{
	return path_item_type(item) <= PIT_STADYN && item != PIT_BAD;
}

constexpr bool is_path_item_dynamic(size_t item)
{
	return path_item_type(item) >= PIT_STADYN && path_item_type(item) < PIT_OPTIONAL;
}

constexpr bool is_path_item_dyn_size_pos(size_t item)
{
	return path_item_type(item) == PIT_DYN_POS;
}

template <class PATH, enum path_item_type TYPE,
	size_t STATIC_SIZE = 0, size_t STATIC_POS = 0>
using path_push_t =
	typename PATH::template add_back_t<path_item_new(TYPE,
							 STATIC_SIZE,
							 STATIC_POS)>;

template <size_t I, size_t... P>
struct Resolver {
	template <size_t J>
	static constexpr size_t ITEM = tnt::iseq<P...>::template get<J>();
	static constexpr size_t P0 = ITEM<0>;
	static constexpr size_t PI = ITEM<I>;
	static constexpr enum path_item_type TYPE = path_item_type(PI);
	static constexpr size_t POS = path_item_static_pos(PI);
	static constexpr size_t SIZE = path_item_static_size(PI);
	static constexpr size_t USR_ARG_COUNT = path_item_static_size(P0);
	static_assert(path_item_type(P0) == PIT_STATIC_L0);
	/* Is set if the current path item requires static POS and SIZE. */
	static constexpr bool requires_pos_and_size =
		is_path_item_static(PI) || path_item_type(PI) == PIT_VARIANT;
	static_assert((requires_pos_and_size && POS < SIZE) ||
		      (!requires_pos_and_size && POS + SIZE == 0));

	template <size_t... J>
	static constexpr size_t dyn_arg_pos(tnt::iseq<J...>)
	{
		constexpr size_t X =
			((is_path_item_dynamic(ITEM<J>) ? 1 : 0) + ... + 0);
		return USR_ARG_COUNT + X;
	}

	static constexpr size_t dyn_arg_pos()
	{
		return dyn_arg_pos(tnt::make_iseq<I>{});
	}

	template <class... T>
	static constexpr size_t find_obj_index()
	{
		using E = decltype(extract<T...>(std::declval<T>()...));
		using R = unwrap_t<E>;
		if constexpr (has_dec_rule_v<R>) {
			return I;
		} else if constexpr (std::is_member_pointer_v<R> ||
				     tnt::is_tuplish_v<R>) {
			using PrevResolver = Resolver<I - 1, P...>;
			return PrevResolver::template find_obj_index<T...>();
		} else {
			static_assert(tnt::always_false_v<E>);
		}
	}

	template <class... T>
	static constexpr auto&& prev(T... t)
	{
		return unwrap(Resolver<I - 1, P...>::get(t...));
	}

	template <class... T>
	static constexpr auto&& extract(T... t)
	{
		if constexpr (TYPE == PIT_STATIC_L0) {
			return std::get<POS>(std::tie(t...)).get();
		} else if constexpr (is_path_item_static(PI)) {
			return tnt::get<POS>(prev(t...));
		} else if constexpr (TYPE == PIT_DYN_POS) {
			constexpr size_t ARG_POS = dyn_arg_pos();
			uint64_t arg = std::get<ARG_POS>(std::tie(t...));
			return std::data(prev(t...))[arg >> 32];
		} else if constexpr (TYPE == PIT_DYN_BACK) {
			return prev(t...).back();
		} else if constexpr (TYPE == PIT_DYN_ADD) {
			return prev(t...);
		} else if constexpr (TYPE == PIT_DYN_KEY) {
			return prev(t...);
		} else if constexpr (TYPE == PIT_OPTIONAL) {
			auto &&opt = prev(t...);
			assert(opt.has_value());
			return *opt;
		} else if constexpr (TYPE == PIT_VARIANT) {
			return tnt::get<POS>(prev(t...));
		} else {
			static_assert(tnt::always_false_v<T...>);
		}
	}

	template <class... T>
	static constexpr auto&& unrule(T... t)
	{
		using E = decltype(extract<T...>(std::declval<T>()...));
		using R = unwrap_t<E>;
		if constexpr (has_dec_rule_v<R>)
			return get_dec_rule<R>();
		else
			return extract(t...);
	}

	template <class T>
	static constexpr auto&& self_unwrap(T&& t)
	{
		using R = unwrap_t<T>;
		if constexpr (has_dec_rule_v<R>) {
			using RULE = unwrap_t<decltype(get_dec_rule<R>())>;
			if constexpr (std::is_member_pointer_v<RULE>) {
				return self_unwrap(unwrap(std::forward<T>(t)).*
						   unwrap(get_dec_rule<R>()));
			} else {
				return std::forward<T>(t);
			}
		} else {
			return std::forward<T>(t);
		}
	}

	template <class... T>
	static constexpr auto&& get(T... t)
	{
		using U = decltype(unrule<T...>(std::declval<T>()...));
		using R = unwrap_t<U>;
		if constexpr (std::is_member_pointer_v<R>) {
			constexpr size_t OBJ_I = find_obj_index<T...>();
			using Res = Resolver<OBJ_I, P...>;
			return self_unwrap(unwrap(Res::extract(t...)).*
					   unwrap(unrule(t...)));
		} else {
			return unrule(t...);
		}
	}

	static constexpr size_t expected_arg_count()
	{
		return dyn_arg_pos() + (is_path_item_dynamic(ITEM<I>) ? 1 : 0);
	}
};

template <size_t... P, class... T>
constexpr auto&& path_resolve(tnt::iseq<P...>, T... t)
{
	using Res = Resolver<sizeof...(P) - 1, P...>;
	static_assert(Res::expected_arg_count() == sizeof...(T));
	return Res::get(t...);
}

template <size_t... P, class... T>
constexpr auto&& path_resolve_parent(tnt::iseq<P...>, T... t)
{
	using Res = Resolver<sizeof...(P) - 1, P...>;
	static_assert(Res::expected_arg_count() == sizeof...(T) - 1);
	return Res::get(t...);
}

constexpr size_t SIMPLEX_SUBRULE = 16;

template <compact::Family FAMILY>
constexpr auto get_subrules()
{
	using RULE = rule_by_family_t<FAMILY>;
	using simplex_seq = tnt::iseq<SIMPLEX_SUBRULE>;
	using complex_seq = tnt::make_iseq<rule_complex_count_v<RULE>>;
	if constexpr (RULE::has_simplex)
		return simplex_seq{} + complex_seq{};
	else
		return complex_seq{};
}

template <compact::Family FAMILY, size_t SUBRULE, class BUF>
auto read_value(BUF& buf)
{
	using RULE = rule_by_family_t<FAMILY>;
	if constexpr (SUBRULE == SIMPLEX_SUBRULE) {
		typename RULE::simplex_value_t tag;
		buf.read(tag);
		assert(tag >= rule_simplex_tag_range_v<RULE>.first);
		assert(tag <= rule_simplex_tag_range_v<RULE>.last);
		[[maybe_unused]] typename RULE::simplex_value_t val =
			static_cast<typename RULE::simplex_value_t>(tag - RULE::simplex_tag);

		if constexpr (FAMILY == compact::MP_NIL)
			return nullptr;
		else if constexpr (RULE::is_bool)
			return bool(val);
		else if constexpr (RULE::is_simplex_log_range)
			return 1u << val;
		else
			return val;
	} else {
		uint8_t tag;
		buf.read(tag);
		assert(tag == RULE::complex_tag + SUBRULE);
		using TYPES = typename RULE::complex_types;
		using V = std::tuple_element_t<SUBRULE, TYPES>;
		under_uint_t<V> u;
		buf.read(u);
		V val = bswap<V>(u);
		return val;
	}
}

template <compact::Family FAMILY, size_t SUBRULE, class BUF, class ITEM>
auto read_item(BUF& buf, ITEM& item)
{
	using RULE = rule_by_family_t<FAMILY>;
	auto val = read_value<FAMILY, SUBRULE>(buf);
	if constexpr (RULE::has_ext) {
		int8_t ext_type;
		buf.read(ext_type);
		item.ext_type = ext_type;
	}
	if constexpr (RULE::has_data) {
		size_t size = size_t(val);
		if constexpr (tnt::is_resizable_v<ITEM>) {
			if constexpr (tnt::is_limited_v<ITEM>) {
				if (size > ITEM::static_capacity) {
					item.resize(ITEM::static_capacity);
					size = ITEM::static_capacity;
				} else {
					item.resize(size);
				}
			} else {
				item.resize(size);
			}
		} else if constexpr (tnt::is_limited_v<ITEM>) {
			if (size > ITEM::static_capacity)
				size = ITEM::static_capacity;
		} else {
			if (size > std::size(item))
				size = std::size(item);
		}
		buf.read({std::data(item), size});
		if constexpr (tnt::is_limited_v<ITEM> ||
			      !tnt::is_resizable_v<ITEM>) {
			if (size < size_t(val))
				buf.read({size_t(val) - size});
		}
	} else if constexpr (RULE::has_children) {
		if constexpr (tnt::is_clearable_v<ITEM>)
			item.clear();
		else if constexpr (tnt::is_resizable_v<ITEM>)
			item.resize(val);
	} else if constexpr (std::is_enum_v<ITEM>) {
		item = static_cast<ITEM>(val);
	} else if constexpr (tnt::is_optional_v<ITEM> && FAMILY == compact::MP_NIL) {
		item.reset();
	} else {
		item = static_cast<ITEM>(val);
	}
	return val;
}

template <class BUF, class... T>
using jump_common_t = bool (*)(BUF&, T...);

template <class BUF, class... T>
struct Jumps {
	using jump_t = jump_common_t<BUF, T...>;
	using data_t = std::array<jump_t, 256>;
	data_t data;

	constexpr Jumps() : data{} {}

	/** Create by jump function and tag range. */
	static constexpr bool
	belongs(uint8_t i, uint8_t n, uint8_t x)
	{
		return n <= x ? (i >= n && i <= x) : (i >= n || i <= x);
	}

	template <size_t... I>
	static constexpr data_t
	build_by_range(jump_t jump, uint8_t min_tag, uint8_t max_tag, tnt::iseq<I...>)
	{
		return {(belongs(I, min_tag, max_tag) ? jump : nullptr)...};
	}

	constexpr Jumps(jump_t jump, uint8_t min_tag, uint8_t max_tag)
		: data(build_by_range(jump, min_tag, max_tag, is256))
	{
	}

	/** Create as union of two jump vectors. */
	static constexpr jump_t
	choose(jump_t a, jump_t b)
	{
		return a != nullptr ? a : b;
	}

	template <size_t... I>
	static constexpr data_t
	build_choosing(data_t a, data_t b, tnt::iseq<I...>)
	{
		return {choose(a[I], b[I])...};
	}

	constexpr Jumps(Jumps a, Jumps b)
		: data(build_choosing(a.data, b.data, is256))
	{
	}

	friend constexpr Jumps operator+(Jumps a, Jumps b)
	{
		return {a, b};
	}

	/** Override given tag with special jump. */
	template <size_t... I>
	static constexpr data_t
	build_inject(data_t orig, uint8_t tag, jump_t jump, tnt::iseq<I...>)
	{
		return {(I != tag ? orig[I] : jump)...};
	}

	constexpr Jumps(Jumps a, uint8_t tag, jump_t jump)
		: data(build_inject(a.data, tag, jump, is256))
	{
	}

	constexpr Jumps inject(uint8_t tag, jump_t jump) const
	{
		return {*this, tag, jump};
	}

	/** Finalize replacing null jumps with default jump. */
	template <size_t... I>
	static constexpr data_t
	build_default(data_t a, jump_t deflt, tnt::iseq<I...>)
	{
		return {choose(a[I], deflt)...};
	}

	constexpr Jumps(Jumps a, jump_t deflt)
		: data(build_default(a.data, deflt, is256))
	{
	}

	constexpr Jumps finalize(jump_t deflt) const
	{
		return {*this, deflt};
	}
};

template <compact::Family FAMILY, size_t SUBRULE,
	class PATH, class BUF, class... T>
bool jump_common(BUF& buf, T... t);

template <class BUF, class... T>
bool unexpected_type_jump(BUF&, T...);

template <class BUF, class... T>
bool broken_msgpack_jump(BUF&, T...);

template <class PATH, class BUF, class... T>
struct JumpsBuilder {
	template <compact::Family FAMILY, size_t SUBRULE>
	static constexpr auto build()
	{
		using RULE = rule_by_family_t<FAMILY>;
		constexpr jump_common_t<BUF, T...> j =
			jump_common<FAMILY, SUBRULE, PATH, BUF, T...>;
		if constexpr (SUBRULE == SIMPLEX_SUBRULE) {
			constexpr auto t = RULE::simplex_tag;
			constexpr uint8_t f = static_cast<uint8_t>(RULE::simplex_value_range.first);
			constexpr uint8_t l = static_cast<uint8_t>(RULE::simplex_value_range.last);
			return Jumps<BUF, T...>{j, t + f, t + l};
		} else {
			constexpr auto t = RULE::complex_tag;
			return Jumps<BUF, T...>{j, t + SUBRULE, t + SUBRULE};
		}
	}

	template <compact::Family FAMILY, size_t... SUBRULE>
	static constexpr auto build(tnt::iseq<SUBRULE...>)
	{
		return (build<FAMILY, SUBRULE>() + ...);
	}

	template <compact::Family... FAMILY>
	static constexpr auto build(family_sequence<FAMILY...> s)
	{
		if constexpr (s.size() == 0)
			return Jumps<BUF, T...>{};
		else
			return (build<FAMILY>(get_subrules<FAMILY>()) + ...);
	}

	template <class V>
	static constexpr bool is_good_key()
	{
		using U = unwrap_t<V>;
		if constexpr (tnt::is_integral_constant_v<U>) {
			return tnt::is_integer_v<typename U::value_type>;
		} else {
			return tnt::is_integer_v<U> ||
			       tnt::is_string_constant_v<U> ||
			       tnt::is_char_ptr_v<U> ||
			       tnt::is_contiguous_char_v<U>;
		}
	}

	template <class V>
	static constexpr bool is_int_key()
	{
		using U = unwrap_t<V>;
		return tnt::is_integral_constant_v<U> || tnt::is_integer_v<U>;
	}

	template <class V>
	static constexpr bool is_str_key()
	{
		using U = unwrap_t<V>;
		return tnt::is_string_constant_v<U> ||
		       tnt::is_char_ptr_v<U> ||
		       tnt::is_contiguous_char_v<U>;
	}

	template <class TYPES, size_t... I>
	static constexpr auto keyMapKeyFamiliesCommon()
	{
		constexpr bool is_good =
			(is_good_key<tnt::tuple_element_t<I, TYPES>>() && ...);
		static_assert(is_good);
		constexpr bool has_int =
			(is_int_key<tnt::tuple_element_t<I, TYPES>>() || ...);
		constexpr bool has_str =
			(is_str_key<tnt::tuple_element_t<I, TYPES>>() || ...);
		if constexpr (has_int && has_str)
			return family_sequence<compact::MP_INT, compact::MP_STR>{};
		else if constexpr (has_int)
			return family_sequence<compact::MP_INT>{};
		else if constexpr (has_str)
			return family_sequence<compact::MP_STR>{};
		else
			return family_sequence<>{};
	}

	template <class DST, size_t... I>
	static constexpr auto keyMapKeyFamiliesFlat(tnt::iseq<I...>)
	{
		using TYPES = std::tuple<tnt::tuple_element_t<I * 2, DST>...>;
		return keyMapKeyFamiliesCommon<TYPES, I...>();
	}

	template <class DST, size_t... I>
	static constexpr auto keyMapKeyFamiliesPairs(tnt::iseq<I...>)
	{
		using TYPES = std::tuple<tnt::tuple_element_t<0,
			std::remove_reference_t<tnt::tuple_element_t<I, DST>>>...>;
		return keyMapKeyFamiliesCommon<TYPES, I...>();
	}

	static constexpr auto prepareFamilySequence()
	{
		constexpr size_t LAST = PATH::last();
		if constexpr (path_item_type(LAST) == PIT_DYN_SKIP) {
			return getFamiliesByRules<all_rules_t>();
		} else if constexpr (path_item_type(LAST) == PIT_DYN_KEY) {
			using R = decltype(path_resolve(PATH{},
							std::declval<T>()...));
			using DST = std::remove_reference_t<R>;
			static_assert(tnt::is_tuplish_v<DST>);
			constexpr size_t S = tnt::tuple_size_v<DST>;
			if constexpr (tnt::is_tuplish_of_pairish_v<DST>) {
				using is = tnt::make_iseq<S>;
				return keyMapKeyFamiliesPairs<DST>(is{});
			} else {
				static_assert(S % 2 == 0);
				using is = tnt::make_iseq<S / 2>;
				return keyMapKeyFamiliesFlat<DST>(is{});
			}
		} else {
			using R = decltype(path_resolve(PATH{},
							std::declval<T>()...));
			using DST = std::remove_reference_t<R>;
			if constexpr (path_item_type(LAST) == PIT_DYN_ADD) {
				return detectFamily<BUF, tnt::value_type_t<DST>>();
			} else {
				return detectFamily<BUF, DST>();
			}
		}
	}

	static constexpr auto build()
	{
		constexpr auto fseq = prepareFamilySequence();
		constexpr auto prepared = build(fseq);
		constexpr auto valid =
			prepared.inject(IgnrRule::simplex_tag,
					broken_msgpack_jump<BUF, T...>);
		return valid.finalize(unexpected_type_jump<BUF, T...>);
	}
};

template <class PATH, class BUF, class... T>
bool decode_impl(BUF& buf, T... t);

/**
 * Bulids a jump table and jumps by a current byte in the buffer.
 */
template <class PATH, class BUF, class... T>
bool
decode_jump(BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) != PIT_BAD);
	static constexpr auto jumps = JumpsBuilder<PATH, BUF, T...>::build();
	uint8_t tag = buf.template get<uint8_t>();
	return jumps.data[tag](buf, t...);
}

/**
 * Saves an iterator to the beginning of object and modifies path to rewind
 * buf to the end of current object and save an iterator to it.
 */
template <class PATH, class BUF, class... T>
bool
decode_raw(BUF& buf, T... t)
{
	auto&& dst = unwrap(path_resolve(PATH{}, t...));
	using dst_t = std::remove_reference_t<decltype(dst)>;
	if constexpr (tnt::is_pairish_of_v<dst_t, BUF, BUF>)
		dst.first = buf;
	else
		static_assert(tnt::always_false_v<dst_t>);
	/*
	 * Let's populate path with PIT_RAW to save the second
	 * iterator when it will be popped and with PIT_DYN_SKIP
	 * to actually skip the current object.
	 */
	using RAW_PATH = path_push_t<PATH, PIT_RAW>;
	using RAW_SKIP_PATH = path_push_t<RAW_PATH, PIT_DYN_SKIP>;
	return decode_impl<RAW_SKIP_PATH>(buf, t..., size_t(1));
}

/**
 * A central function of the decoder.
 * The decoding of each object starts from here.
 */
template <class PATH, class BUF, class... T>
bool
decode_impl(BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) != PIT_BAD);
	if constexpr (path_item_type(PATH::last()) != PIT_DYN_SKIP &&
		      path_item_type(PATH::last()) != PIT_RAW) {
		auto&& wrapped_dst = path_resolve(PATH{}, t...);
		using wrapped_dst_t = std::remove_reference_t<decltype(wrapped_dst)>;
		if constexpr (is_raw_decoded_v<wrapped_dst_t, BUF>) {
			return decode_raw<PATH>(buf, t...);
		} else {
			return decode_jump<PATH>(buf, t...);
		}
	} else {
		return decode_jump<PATH>(buf, t...);
	}
}

template <class BUF, class... T>
bool
decode(BUF& buf, T&&... t)
{
	constexpr size_t P = path_item_new(PIT_STATIC_L0, sizeof...(T));
	return decode_impl<tnt::iseq<P>>(buf, std::ref(t)...);
}

template <class PATH, class BUF, class... T>
bool decode_next(BUF& buf, T... t);

template <class PATH, size_t... I, class BUF, class... T>
bool decode_next_drop_arg_impl(tnt::iseq<I...>, BUF& buf, T... t)
{
	return decode_next<PATH>(buf, std::get<I>(std::tuple(t...))...);
}

template <class PATH, class BUF, class... T>
bool decode_next_drop_arg(BUF& buf, T... t)
{
	using iseq = tnt::make_iseq<sizeof...(t) - 1>;
	return decode_next_drop_arg_impl<PATH>(iseq{}, buf, t...);
}

template <class PATH, class BUF, class... T>
bool decode_next(BUF& buf, T... t)
{
	if constexpr (PATH::size() == 0)
		return true;
	else if constexpr (path_item_type(PATH::last()) == PIT_RAW) {
		using POP_PATH = typename PATH::pop_back_t;
		auto&& wrapped_dst = path_resolve(POP_PATH{}, t...);
		using wrapped_dst_t = std::remove_reference_t<decltype(wrapped_dst)>;
		static_assert(is_raw_decoded_v<wrapped_dst_t, BUF>);
		auto&& dst = unwrap(wrapped_dst);
		using dst_t = std::remove_reference_t<decltype(dst)>;
		if constexpr (tnt::is_pairish_of_v<dst_t, BUF, BUF>) {
			dst.second = buf;
		} else {
			static_assert(tnt::always_false_v<dst_t>);
		}
		return decode_next<POP_PATH>(buf, t...);
	} else {
		constexpr size_t LAST = PATH::last();
		constexpr enum path_item_type LAST_TYPE = path_item_type(LAST);
		constexpr bool has_static = is_path_item_static(LAST);
		using POP_PATH = typename PATH::pop_back_t;
		using NEXT_PATH =
			std::conditional_t<has_static,
					   typename PATH::inc_back_t, PATH>;
		constexpr size_t NEXT_LAST = NEXT_PATH::last();
		constexpr bool static_done =
			has_static && path_item_static_size(NEXT_LAST) ==
				      path_item_static_pos(NEXT_LAST);

		[[maybe_unused]] bool is_dyn_done = false;
		if constexpr (LAST_TYPE == PIT_DYN_POS) {
			auto& arg = std::get<sizeof...(T) - 1>(std::tie(t...));
			assert((arg & 0xFFFFFFFF) != 0);
			// Increment high part (pos) and decrement low (count).
			is_dyn_done = ((arg += 0xFFFFFFFF) & 0xFFFFFFFF) == 0;
		} else if constexpr (is_path_item_dynamic(LAST)) {
			auto& arg = std::get<sizeof...(T) - 1>(std::tie(t...));
			assert(arg != 0);
			is_dyn_done = --arg == 0;
		}

		[[maybe_unused]] bool is_dyn_full = false;
		if constexpr (LAST_TYPE == PIT_DYN_POS ||
			      LAST_TYPE == PIT_DYN_ADD ||
			      LAST_TYPE == PIT_DYN_BACK) {
			auto&& parent = path_resolve_parent(POP_PATH{}, t...);
			using par_t = std::remove_reference_t<decltype(parent)>;
			auto& arg = std::get<sizeof...(T) - 1>(std::tie(t...));
			[[maybe_unused]] size_t pos = arg >> 32;
			if constexpr (LAST_TYPE == PIT_DYN_POS &&
				      tnt::is_limited_v<par_t>) {
				is_dyn_full = pos >= par_t::static_capacity;
			} else if constexpr (LAST_TYPE == PIT_DYN_POS &&
					     tnt::is_tuplish_v<par_t>) {
				is_dyn_full = pos >= tnt::tuple_size_v<par_t>;
			} else if constexpr (LAST_TYPE == PIT_DYN_POS &&
					     tnt::is_sizable_v<par_t> &&
					     !tnt::is_resizable_v<par_t>) {
				is_dyn_full = pos >= std::size(parent);
			} else if constexpr (LAST_TYPE != PIT_DYN_POS &&
					     tnt::is_sizable_v<par_t> &&
					     tnt::is_limited_v<par_t>) {
				pos = std::size(parent);
				is_dyn_full = pos >= par_t::static_capacity;
			}
		}

		if constexpr (static_done && is_path_item_dynamic(LAST)) {
			assert(!is_dyn_done);
			static_assert(LAST_TYPE == PIT_STADYN);
			using SKIP_PATH = path_push_t<POP_PATH, PIT_DYN_SKIP>;
			return decode_impl<SKIP_PATH>(buf, t...);
		} else if constexpr (static_done) {
			return decode_next<POP_PATH>(buf, t...);
		} else if constexpr (LAST_TYPE == PIT_OPTIONAL || LAST_TYPE == PIT_VARIANT) {
			return decode_next<POP_PATH>(buf, t...);
		} else if constexpr (!is_path_item_dynamic(LAST)) {
			return decode_impl<NEXT_PATH>(buf, t...);
		} else if (is_dyn_done) {
			return decode_next_drop_arg<POP_PATH>(buf, t...);
		} else if (is_dyn_full) {
			auto& arg = std::get<sizeof...(T) - 1>(std::tie(t...));
			// Remove high part (pos) and leave low part (count).
			arg &= 0xFFFFFFFF;
			using SKIP_PATH = path_push_t<POP_PATH, PIT_DYN_SKIP>;
			return decode_impl<SKIP_PATH>(buf, t...);
		} else {
			return decode_impl<NEXT_PATH>(buf, t...);
		}
	}
}

template <compact::Family FAMILY, size_t SUBRULE,
	class PATH, class BUF, class... T>
bool jump_skip(BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) == PIT_DYN_SKIP);
	using RULE = rule_by_family_t<FAMILY>;
	[[maybe_unused]] auto val = read_value<FAMILY, SUBRULE>(buf);

	if constexpr (RULE::has_ext) {
		int8_t ext_type;
		buf.read(ext_type);
	}
	if constexpr (RULE::has_data) {
		buf.read({size_t(val)});
	}
	if constexpr (RULE::has_children) {
		auto& arg = std::get<sizeof...(T) - 1>(std::tie(t...));
		arg += val * RULE::children_multiplier;
	}
	return decode_next<PATH>(buf, t...);
}

template <compact::Family FAMILY, size_t SUBRULE,
	class PATH, class BUF, class... T>
bool jump_add(BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) == PIT_DYN_ADD);
	auto&& dst = path_resolve(PATH{}, t...);
	using dst_t = std::remove_reference_t<decltype(dst)>;
	tnt::value_type_t<dst_t> trg;
	read_item<FAMILY, SUBRULE>(buf, trg);

	if constexpr (is_any_putable_v<dst_t>)
		put_to_putable(dst, std::move(trg));
	else
		static_assert(tnt::always_false_v<dst_t>);
	return decode_next<PATH>(buf, t...);
}

template <class BUF, class ARR>
constexpr enum path_item_type get_next_arr_item_type()
{
	if constexpr (tnt::is_contiguous_v<ARR>) {
		return PIT_DYN_POS;
	} else if constexpr (is_any_putable_v<ARR> &&
			     !hasChildren<BUF, tnt::value_type_t<ARR>>()) {
		return PIT_DYN_ADD;
	} else if constexpr (is_any_putable_v<ARR> &&
			     tnt::is_back_accessible_v<ARR>) {
		return PIT_DYN_BACK;
	} else if constexpr (tnt::is_tuplish_v<ARR>) {
		if constexpr (tnt::tuple_size_v<ARR> == 0)
			return PIT_DYN_SKIP;
		else
			return PIT_STADYN;
	} else {
		return PIT_BAD;
	}
}

template <class ARR, enum path_item_type TYPE>
constexpr size_t get_next_arr_static_size()
{
	if constexpr (TYPE <= PIT_STADYN && TYPE != PIT_BAD)
		return tnt::tuple_size_v<ARR>;
	else
		return 0;
}

template <compact::Family FAMILY, size_t SUBRULE,
	  class PATH, class BUF, class... T>
bool jump_read(BUF& buf, T... t)
{
	auto&& dst = unwrap(path_resolve(PATH{}, t...));
	using dst_t = std::remove_reference_t<decltype(dst)>;
	using RULE = rule_by_family_t<FAMILY>;
	auto val = read_item<FAMILY, SUBRULE>(buf, dst);
	if (RULE::has_children && val == 0)
		goto decode_next_label;

	if constexpr (FAMILY == compact::MP_ARR) {
		constexpr auto NT = get_next_arr_item_type<BUF, dst_t>();
		constexpr size_t NS = get_next_arr_static_size<dst_t, NT>();
		using NEXT_PATH = path_push_t<PATH, NT, NS>;
		if constexpr (NT == PIT_BAD) {
			// Failed to find an optimized way, use simple cycle.
			static_assert(is_any_putable_v <dst_t>);
			for (size_t i = 0; i < size_t(val); i++) {
				tnt::value_type_t<dst_t> trg;
				if (!decode(buf, trg))
					return false;
				put_to_putable(dst, std::move(trg));
			}
		} else {
			if constexpr (NT == PIT_STADYN) {
				using ALTER = path_push_t<PATH, PIT_STATIC, NS>;
				if (val == NS) {
					// Static-only size optimization.
					return decode_impl<ALTER>(buf, t...);
				}
			} else if constexpr (NT == PIT_DYN_POS) {
				if constexpr (tnt::is_limited_v<dst_t> &&
					      tnt::is_resizable_v<dst_t>) {
					if (val > dst_t::static_capacity)
						dst.resize(dst_t::static_capacity);
					else
						dst.resize(val);
				} else if constexpr (tnt::is_resizable_v<dst_t>) {
					dst.resize(val);
				}
			}
			uint64_t arg = val;
			return decode_impl<NEXT_PATH>(buf, t..., arg);
		}
	} else if constexpr (FAMILY == compact::MP_MAP) {
		if constexpr (tnt::is_tuplish_v<dst_t>) {
			uint64_t arg = val;
			using NEXT_PATH = path_push_t<PATH, PIT_DYN_KEY>;
			return decode_impl<NEXT_PATH>(buf, t..., arg);
		} else {
			// Failed to find an optimized way, use simple cycle.
			static_assert(is_any_putable_v<dst_t>);
			for (size_t i = 0; i < size_t(val); i++) {
				using V = tnt::value_type_t<dst_t>;
				using V1C = decltype(std::declval<V>().first);
				using V1 = std::remove_const_t<V1C>;
				using V2 = decltype(std::declval<V>().second);
				V1 first;
				V2 second;
				if (!decode(buf, first, second))
					return false;
				V trg{std::move(first), std::move(second)};
				if constexpr (tnt::is_contiguous_v<dst_t>)
					std::data(dst)[i] = std::move(trg);
				else
					put_to_putable(dst, std::move(trg));
			}
		}
	}

decode_next_label:
	return decode_next<PATH>(buf, t...);
}

template <class K, class V>
constexpr bool signed_compare(K k, V v)
{
	if constexpr (std::is_signed_v<K> == std::is_signed_v<V>) {
		return k == v;
	} else if constexpr (std::is_signed_v<K>) {
		return k < 0 ? false :
		       static_cast<std::make_unsigned_t<K>>(k) == v;
	} else {
		static_assert(std::is_signed_v<V>);
		return v < 0 ? false :
		       k == static_cast<std::make_unsigned_t<V>>(v);
	}
}

template <compact::Family FAMILY, class K, class W, class BUF>
constexpr bool compare_key([[maybe_unused]] K k, W&& w, [[maybe_unused]] BUF& buf)
{
	auto&& u = mpp::unwrap(w);
	using U = mpp::unwrap_t<W>;
	static_assert(std::is_integral_v<K>);
	static_assert(!std::is_same_v<K, bool>);
	if constexpr (FAMILY == compact::MP_INT) {
		if constexpr (tnt::is_integral_constant_v<U>) {
			using V = typename U::value_type;
			constexpr tnt::base_enum_t<V> v = U::value;
			return signed_compare(k, v);
		} else if constexpr (tnt::is_integer_v<U>) {
			tnt::base_enum_t<U> v = u;
			return signed_compare(k, v);
		} else {
			return false;
		}
	} else {
		static_assert(FAMILY == compact::MP_STR);
		if constexpr (tnt::is_string_constant_v<U>) {
			return k == u.size && buf.startsWith({u.data, u.size});
		} else if constexpr (tnt::is_char_ptr_v<U>) {
			return k == strlen(u) && buf.startsWith({u, strlen(u)});
		} else if constexpr (tnt::is_contiguous_char_v<U> &&
				     tnt::is_bounded_array_v<U>) {
			/* Note special rule for string literals. */
			return k == strlen(u) && buf.startsWith({u, strlen(u)});
		} else if constexpr (tnt::is_contiguous_char_v<U>) {
			return k == std::size(u) &&
			       buf.startsWith({std::data(u), std::size(u)});
		} else {
			return false;
		}
	}
}

template <bool PAIRS, compact::Family FAMILY, class PATH, class K,
          class BUF, class... T>
bool jump_find_key([[maybe_unused]] K k, tnt::iseq<>, BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) == PIT_DYN_KEY);
	using NEXT_PATH = path_push_t<PATH, PIT_DYN_SKIP>;
	if constexpr (FAMILY == compact::MP_STR)
		buf.read({k});
	return decode_impl<NEXT_PATH>(buf, t..., size_t(1));
}

template <bool PAIRS, size_t I, class DST>
auto&& key_path_resolve(DST&& dst)
{
	if constexpr (PAIRS)
		return tnt::get<0>(tnt::get<I>(dst));
	else
		return tnt::get<I * 2>(dst);
}

template <bool PAIRS, compact::Family FAMILY, class PATH, class K,
	  size_t I, size_t... J, class BUF, class... T>
bool jump_find_key(K k, tnt::iseq<I, J...>, BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) == PIT_DYN_KEY);

	auto&& key = key_path_resolve<PAIRS, I>(path_resolve(PATH{}, t...));
	using PAIRS_NEXT_PREPATH = path_push_t<PATH, PIT_STATIC, I + 1, I>;
	using PAIRS_NEXT_PATH = path_push_t<PAIRS_NEXT_PREPATH, PIT_STATIC, 2, 1>;
	using FLAT_NEXT_PATH = path_push_t<PATH, PIT_STATIC, I * 2 + 2, I * 2 + 1>;
	using NEXT_PATH = std::conditional_t<PAIRS, PAIRS_NEXT_PATH, FLAT_NEXT_PATH>;

	if (compare_key<FAMILY>(k, key, buf)) {
		if constexpr (FAMILY == compact::MP_STR)
			buf.read({k});
		return decode_impl<NEXT_PATH>(buf, t...);
	}

	constexpr auto IS = tnt::iseq<J...>{};
	return jump_find_key<PAIRS, FAMILY, PATH>(k, IS, buf, t...);
}

template <compact::Family FAMILY, size_t SUBRULE,
	  class PATH, class BUF, class... T>
bool jump_read_key(BUF& buf, T... t)
{
	static_assert(path_item_type(PATH::last()) == PIT_DYN_KEY);
	auto&& dst = path_resolve(PATH{}, t...);
	using dst_t = std::remove_reference_t<decltype(dst)>;
	static_assert(tnt::is_tuplish_v<dst_t>);

	constexpr bool PAIRS = tnt::is_tuplish_of_pairish_v<dst_t>;
	constexpr size_t TS = tnt::tuple_size_v<dst_t>;
	static_assert(PAIRS || TS % 2 == 0);
	constexpr size_t S = PAIRS ? TS : TS / 2;
	constexpr auto IS = tnt::make_iseq<S>{};

	static_assert(FAMILY == compact::MP_INT || FAMILY == compact::MP_STR);
	auto val = read_value<FAMILY, SUBRULE>(buf);

	return jump_find_key<PAIRS, FAMILY, PATH>(val, IS, buf, t...);
}

template <compact::Family FAMILY, size_t SUBRULE,
	  class PATH, class BUF, class... T>
bool jump_read_optional(BUF& buf, T... t)
{
	auto&& dst = unwrap(path_resolve(PATH{}, t...));
	using dst_t = std::remove_reference_t<decltype(dst)>;
	static_assert(tnt::is_optional_v<dst_t>);
	if constexpr (FAMILY == compact::MP_NIL) {
		[[maybe_unused]] auto val = read_value<FAMILY, SUBRULE>(buf);
		dst.reset();
		return decode_next<PATH>(buf, t...);
	} else {
		if (!dst.has_value())
			dst.emplace();
		using NEXT_PATH = path_push_t<PATH, PIT_OPTIONAL>;
		return decode_impl<NEXT_PATH>(buf, t...);
	}
}

template <size_t I, compact::Family FAMILY, size_t SUBRULE,
	  class PATH, class BUF, class... T>
bool jump_read_variant_impl(BUF& buf, T... t)
{
	auto&& variant = unwrap(path_resolve(PATH{}, t...));
	using variant_t = std::remove_reference_t<decltype(variant)>;
	constexpr size_t size = std::variant_size_v<variant_t>;
	static_assert(tnt::is_variant_v<variant_t>);
	static_assert(I < size);
	using curType = std::variant_alternative_t<I, variant_t>;
	constexpr auto curFamilies = detectFamily<BUF, curType>();

	if constexpr (family_sequence_contains<FAMILY>(curFamilies)) {
		variant.template emplace<I>();
		using NEXT_PATH = path_push_t<PATH, PIT_VARIANT, size, I>;
		return decode_impl<NEXT_PATH>(buf, t...);
	} else {
		return jump_read_variant_impl<I + 1, FAMILY, SUBRULE, PATH>(buf, t...);
	}
}

template <compact::Family FAMILY, size_t SUBRULE,
	  class PATH, class BUF, class... T>
bool jump_read_variant(BUF& buf, T... t)
{
	auto&& dst = unwrap(path_resolve(PATH{}, t...));
	using dst_t = std::remove_reference_t<decltype(dst)>;
	static_assert(tnt::is_variant_v<dst_t>);
	return jump_read_variant_impl<0, FAMILY, SUBRULE, PATH>(buf, t...);
}

template <compact::Family FAMILY, size_t SUBRULE,
	  class PATH, class BUF, class... T>
bool jump_common(BUF& buf, T... t)
{
	if constexpr (path_item_type(PATH::last()) == PIT_DYN_SKIP) {
		return jump_skip<FAMILY, SUBRULE, PATH>(buf, t...);
	} else {
		auto&& dst = unwrap(path_resolve(PATH{}, t...));
		using dst_t = std::remove_reference_t<decltype(dst)>;
		if constexpr (tnt::is_optional_v<dst_t>)
			return jump_read_optional<FAMILY, SUBRULE, PATH>(buf, t...);
		else if constexpr (tnt::is_variant_v<dst_t>)
			return jump_read_variant<FAMILY, SUBRULE, PATH>(buf, t...);
		else if constexpr (path_item_type(PATH::last()) == PIT_DYN_ADD)
			return jump_add<FAMILY, SUBRULE, PATH>(buf, t...);
		else if constexpr (path_item_type(PATH::last()) == PIT_DYN_KEY)
			return jump_read_key<FAMILY, SUBRULE, PATH>(buf, t...);
		else
			return jump_read<FAMILY, SUBRULE, PATH>(buf, t...);
	}
}

template <class BUF, class... T>
bool unexpected_type_jump(BUF&, T...)
{
	return false;
}

template <class BUF, class... T>
bool broken_msgpack_jump(BUF&, T...)
{
	return false;
}

} // namespace decode_details

template <class BUF, class... T>
bool
decode(BUF& buf, T&&... t)
{
	// TODO: Guard
	bool res = decode_details::decode(buf, std::forward<T>(t)...);
	return res;
}

} // namespace mpp
