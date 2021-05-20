#pragma once
/*
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
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

#include <cstdint>
#include <tuple>
#include <limits>

#include "Constants.hpp"

/**
 * A set of rules is the msgpack protocol definition in terms of c++ classes.
 * (see also https://github.com/msgpack/msgpack/blob/master/spec.md)
 *
 * Each class describes how one particular object type is encoded/decoded.
 * In terms of msgpack specification each class represents type family.
 * A full set of such classes is enough to encode/decode everything.
 * All of that classes have only static members and typedefs. There's no
 * sense in instantiation of such a class.
 *
 * In order to define all terms here's a general representation of any
 * object that is encoded with msgpack:
 * | Tag | Multi bytes | Ext |      Data      | Child0 | Child1 ... |
 * | <-----Value-----> |
 * | <----------------Token-----------------> |
 * | <---------------------------Object---------------------------> |
 * Tag - first byte of msgpack encoded object, completely describes type of
 *  the object and layout of token. In many cases also contains the value, in
 *  those cases the next `multy byte` part is omitted.
 * Value - numeric part of object. Is the length of data for str/bin/ext,
 *  the length of array for arr, number k-v part for map, and the value itself
 *  for int/bool/float etc.
 * Multi bytes - big-endian bytes of value if the value it not encoded in tag;
 *  can be of size 0,1,2,4,8, which is specified by preceding tag.
 * Ext - additional byte that is present only for msgpack extension family
 *  and describes the type of that extension.
 * Data - byte sequence for str/bin/ext (absent for other families);
 *  the size of data (if it is present!) is equal to preceding value.
 * Token - unit of msgpack stream, the encoded object without children. The
 *  size of token is specified by tag and value.
 * Child0... - msgpack independently encoded elements of array or keys-values
 *  of maps. The number of direct children is preceding value of an array of
 *  2 * preceding value of a map.
 *
 * The important part of msgpack format is that generally the value can be
 * encoded right in tag OR the tag specifies the number of multi bytes nearby,
 * in which the value is encoded. That gives two ways of value encoding, that
 * are called in this file as Simplex (value is encoded in one byte tag) and
 * Complex (value is encoded in multi bytes with a preceding tag, that specifies
 * the number of multi bytes). Note that not every family can be encoded in
 * both ways, for example MP_BOOL can be only simplex encoded, while MP_BIN
 * can only be complex encoded.
 *
 * Each rule class has a base - BaseRule and thus its members (see the class
 * definition for the list of members). In addition the rule may or may not
 * have Simplex and Complex encoding definition. Absence of one or another
 * part declares that the family cannon be encoded in the corresponding way.
 * Simplex part:
 * simplex_ranges - an array of ranges of values that can be encoded in
 *  simplex way. Note that the ranges can have be negative bounds.
 * simplex_tag - the tag by which the first value in the first range is
 *  encoded. The rest values take incrementally sequent tag.
 * Complex part:
 * complex_types - std::tuple with types that are actually put to the
 *  multibyte part.
 * complex_tag - the tag that stands for the first type in complex_types;
 *  the following types take incrementally sequent tag.
 */

namespace mpp {

struct UintRule;

template <class TYPE, compact::Family FAMILY>
struct BaseRule {
	// A type that can hold a value.
	using type = TYPE;
	// Msgpack family.
	static constexpr compact::Family family = FAMILY;
	// The encoded object has data section (of size equal to value).
	static constexpr bool has_data = FAMILY == compact::MP_STR ||
					 FAMILY == compact::MP_BIN ||
					 FAMILY == compact::MP_EXT;
	// The encoded object has ext type byte.
	static constexpr bool has_ext = FAMILY == compact::MP_EXT;
	// The encoded object has children...
	static constexpr bool has_children = FAMILY == compact::MP_ARR ||
					     FAMILY == compact::MP_MAP;
	// ... and the number of children is value * children_multiplier.
	static constexpr uint32_t children_multiplier =
		FAMILY == compact::MP_ARR ? 1 : FAMILY == compact::MP_MAP ? 2 : 0;
	// The rule is supposed to encode negative integers...
	static constexpr bool is_negative = FAMILY == compact::MP_INT;
	// ... and for similar positive the positive_rule should be used.
	using positive_rule = std::conditional_t<is_negative, UintRule, void>;
};

struct NilRule : BaseRule<std::nullptr_t, compact::MP_NIL> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 1}};
	static constexpr uint8_t simplex_tag = 0xc0;
};

struct IgnrRule : BaseRule<decltype(std::ignore), compact::MP_IGNR> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 1}};
	static constexpr uint8_t simplex_tag = 0xc1;
};

struct BoolRule : BaseRule<bool, compact::MP_BOOL> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 2}};
	static constexpr uint8_t simplex_tag = 0xc2;
};

struct UintRule : BaseRule<uint64_t, compact::MP_UINT> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 128}};
	static constexpr uint8_t simplex_tag = 0x00;
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t, uint64_t>;
	static constexpr uint8_t complex_tag = 0xcc;
};

struct IntRule : BaseRule<int64_t, compact::MP_INT> {
	static constexpr std::pair<int8_t, int8_t> simplex_ranges[] = {{-32, 0}};
	static constexpr uint8_t simplex_tag = -32;
	using complex_types = std::tuple<int8_t, int16_t, int32_t, int64_t>;
	static constexpr uint8_t complex_tag = 0xd0;
};

struct FltRule : BaseRule<float, compact::MP_FLT> {
	using complex_types = std::tuple<float>;
	static constexpr uint8_t complex_tag = 0xca;
};

struct DblRule : BaseRule<double, compact::MP_DBL> {
	using complex_types = std::tuple<double>;
	static constexpr uint8_t complex_tag = 0xcb;
};

struct StrRule : BaseRule<uint32_t, compact::MP_STR> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 32}};
	static constexpr uint8_t simplex_tag = 0xa0;
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xd9;
};

struct BinRule : BaseRule<uint32_t, compact::MP_BIN> {
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xc4;
};

struct ArrRule : BaseRule<uint32_t, compact::MP_ARR> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 16}};
	static constexpr uint8_t simplex_tag = 0x90;
	using complex_types = std::tuple<uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xdc;
};

struct MapRule : BaseRule<uint32_t, compact::MP_MAP> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] = {{0, 16}};
	static constexpr uint8_t simplex_tag = 0x80;
	using complex_types = std::tuple<uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xde;
};

struct ExtRule : BaseRule<uint32_t, compact::MP_EXT> {
	static constexpr std::pair<uint8_t, uint8_t> simplex_ranges[] =
		{{1,  3}, {4,  5}, {8,  9}, {16, 17}};
	static constexpr uint8_t simplex_tag = 0xd4;
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xc7;
};

using AllRules_t = std::tuple<NilRule, IgnrRule, BoolRule, UintRule, IntRule,
	FltRule, DblRule, StrRule, BinRule, ArrRule, MapRule, ExtRule>;

/**
 * Check that a rule has simplex and complex encodings.
 */
namespace details {
template <class T, class _ = void>
struct has_simplex_h : std::false_type {};

template <class T>
struct has_simplex_h<T, std::void_t<decltype(T::simplex_tag)>> : std::true_type {};

template <class T, class _ = void>
struct has_complex_h : std::false_type {};

template <class T>
struct has_complex_h<T, std::void_t<decltype(T::complex_tag)>> : std::true_type {};
} // namespace details {

template <class T>
constexpr bool has_simplex_v = details::has_simplex_h<T>::value;

template <class T>
constexpr bool has_complex_v = details::has_complex_h<T>::value;

/**
 * Find a rule by compact::Family.
 */
namespace details {
template <compact::Family FAMILY, class TUPLE>
struct RuleByFamily_h1 { using type = void; };

template <compact::Family FAMILY, class T, class ...U>
struct RuleByFamily_h1<FAMILY, std::tuple<T, U...>> {
	using next = typename RuleByFamily_h1<FAMILY, std::tuple<U...>>::type;
	static_assert(FAMILY != T::family || std::is_same_v<void, next>);
	using type = std::conditional_t<FAMILY == T::family, T, next>;
};

template <compact::Family FAMILY>
struct RuleByFamily_h2 {
	using type = typename RuleByFamily_h1<FAMILY, AllRules_t>::type;
	static_assert(!std::is_same_v<type, void>);
};
} // namespace details {

template <compact::Family FAMILY>
using RuleByFamily_t = typename details::RuleByFamily_h2<FAMILY>::type;

/**
 * Important helper - SimplexRange - that provides range of simplex tags
 * of given rule. That means that in uint8_t sequence a byte that is in
 * range (SimplexRange::first <= byte < SimplexRange::second) is a one-byte
 * encoded value of that rule.
 */
namespace details {
template <class RULE, class _ = void>
struct SimplexIndexSequence { using type = void; };

template <class RULE>
struct SimplexIndexSequence<RULE, std::enable_if_t<has_simplex_v<RULE>, void>> {
	using type = std::make_index_sequence<std::size(RULE::simplex_ranges)>;
};

template <class RULE, class INDEX_SEQUENCE_OR_VOID>
struct SimplexRange_h {
	static constexpr size_t length = 0, first = 0, second = 0;
};

template <class RULE, size_t ...I>
struct SimplexRange_h<RULE, std::index_sequence<I...>> {
	static constexpr size_t length =
		((RULE::simplex_ranges[I].second -
		  RULE::simplex_ranges[I].first) + ...);
	static constexpr size_t first = RULE::simplex_tag;
	static constexpr size_t second = first + length;
};
} // namespace details {

template <class RULE>
struct SimplexRange
	: details::SimplexRange_h<RULE,
		typename details::SimplexIndexSequence<RULE>::type> { };

/**
 * Important helper - ComplexRange - that provides range of complex tags
 * of given rule. That means that in uint8_t sequence a byte that is in
 * range (ComplexRange::first <= byte < ComplexRange::second) is a start of
 * multi-byte encoded value of that rule.
 */
namespace details {
template <class RULE, class _ = void>
struct ComplexRange_h {
	static constexpr size_t length = 0, first = 0, second = 0;
};

template <class RULE>
struct ComplexRange_h<RULE, std::enable_if_t<has_complex_v<RULE>, void>> {
	static constexpr size_t
		length = std::tuple_size_v<typename RULE::complex_types>;
	static constexpr size_t first = RULE::complex_tag;
	static constexpr size_t second = first + length;
};
} // namespace details {

template <class RULE>
struct ComplexRange : details::ComplexRange_h<RULE> { };

/**
 * Try to find an offset of simplex tag by given value.
 * Is SimplexRange<RULE>::length if that is not possible.
 * If success, that value must be encoded with a tag RULE::simplex_tag + result.
 */
namespace details {
template <class RULE, size_t I, size_t N, size_t S, class V>
constexpr size_t find_simplex_offset_h(V value)
{
	if constexpr(std::is_same_v<V, bool> &&
		     RULE::simplex_ranges[I].first == 0 &&
		     RULE::simplex_ranges[I].second >= 2) {
		return S + value - RULE::simplex_ranges[I].first;
	} else {
		if (value >= RULE::simplex_ranges[I].first &&
		    value < RULE::simplex_ranges[I].second)
			return S + value - RULE::simplex_ranges[I].first;
	}
	if constexpr(I + 1 < N) {
		constexpr size_t rlen = RULE::simplex_ranges[I].second -
					RULE::simplex_ranges[I].first;
		return find_simplex_offset_h<RULE, I + 1, N, S + rlen>(value);
	} else {
		return SimplexRange<RULE>::length;
	}
}
} // namespace details {

template <class RULE, class V>
constexpr size_t find_simplex_offset(V value)
{
	if constexpr(!has_simplex_v<RULE>) {
		return SimplexRange<RULE>::length;
	} else {
		constexpr size_t N = sizeof(RULE::simplex_ranges) /
				     sizeof(RULE::simplex_ranges[0]);
		return details::find_simplex_offset_h<RULE, 0, N, 0>(value);
	}
}

/**
 * Try to find an offset of complex tag by given value.
 * The value is expected to be compatible with the rule, comply is_negative
 * and be represented with the largest complex_types.
 * That means that value can be encoded with a tag RULE::complex_tag + result.
 */
namespace details {
template <class RULE, size_t I, size_t N, class V>
constexpr size_t find_complex_offset_h([[maybe_unused]] V x)
{
	if constexpr(I + 1 < N) {
		using type = std::tuple_element_t<I, typename RULE::complex_types>;
		using lim = std::numeric_limits<type>;
		if constexpr (std::is_signed_v<type> == std::is_signed_v<V>) {
			if constexpr(sizeof(type) >= sizeof(V)) {
				return I;
			} else if constexpr(std::is_signed_v<type>) {
				return x >= lim::min() ? I
				: find_complex_offset_h<RULE, I + 1, N>(x);
			} else {
				return x <= lim::max() ? I
				: find_complex_offset_h<RULE, I + 1, N>(x);
			}
		} else if constexpr (std::is_signed_v<type>) {
			if constexpr(sizeof(type) > sizeof(V)) {
				return I;
			} else {
				using utype = std::make_unsigned_t<type>;
				auto max = static_cast<utype>(lim::max());
				return x <= max ? I
				: find_complex_offset_h<RULE, I + 1, N>(x);
			}
		} else {
			if constexpr(sizeof(type) >= sizeof(V)) {
				return I;
			} else {
				auto y = static_cast<std::make_unsigned_t<V>>(x);
				return y <= lim::max() ? I
				: find_complex_offset_h<RULE, I + 1, N>(x);
			}
		}
	} else {
		return I;
	}
}
} // namespace details {

template <class RULE, class V>
constexpr size_t find_complex_offset(V value)
{
	static_assert(has_complex_v<RULE>);
	constexpr size_t N = std::tuple_size_v<typename RULE::complex_types>;
	return details::find_complex_offset_h<RULE, 0, N>(value);
}

/**
 * Check that a rule has an encoded value that starts with given tag.
 */
template <class RULE, uint8_t TAG>
constexpr bool rule_has_simplex_tag_v =
	(TAG >= SimplexRange<RULE>::first && TAG < SimplexRange<RULE>::second);

template <class RULE, uint8_t TAG>
constexpr bool rule_has_complex_tag_v =
	(TAG >= ComplexRange<RULE>::first && TAG < ComplexRange<RULE>::second);

template <class RULE, uint8_t TAG>
constexpr bool rule_has_tag_v =
	rule_has_simplex_tag_v<RULE, TAG> || rule_has_complex_tag_v<RULE, TAG>;

template <class RULE>
constexpr bool rule_has_simplex_tag([[maybe_unused]] uint8_t tag)
{
	if constexpr (has_simplex_v<RULE>)
		return tag >= SimplexRange<RULE>::first &&
		       tag < SimplexRange<RULE>::second;
	return false;
}

template <class RULE>
constexpr bool rule_has_complex_tag([[maybe_unused]] uint8_t tag)
{
	if constexpr (has_complex_v<RULE>)
		return tag >= ComplexRange<RULE>::first &&
		       tag < ComplexRange<RULE>::second;
	return false;
}

template <class RULE>
constexpr bool rule_has_tag(uint8_t tag)
{
	return rule_has_simplex_tag<RULE>(tag) || rule_has_complex_tag<RULE>(tag);
}

/**
 * Find a rule by tag.
 */
namespace details {
template <uint8_t TAG, class TUPLE>
struct RuleByTag_h1 { using type = void; };

template <uint8_t TAG, class T, class ...U>
struct RuleByTag_h1<TAG, std::tuple<T, U...>> {
	using next = typename RuleByTag_h1<TAG, std::tuple<U...>>::type;
	static_assert(!rule_has_tag_v<T, TAG> || std::is_same_v<void, next>);
	using type = std::conditional_t<rule_has_tag_v<T, TAG>, T, next>;
};

template <uint8_t TAG>
struct RuleByTag_h2 {
	using type = typename RuleByTag_h1<TAG, AllRules_t>::type;
	static_assert(!std::is_same_v<type, void>);
};
} // namespace details {

template <uint8_t TAG>
using RuleByTag_t = typename details::RuleByTag_h2<TAG>::type;

/**
 * A tuple that constructed with rules by sequent tags 0..255.
 */
namespace details {
template<class SEQUENCE = std::make_index_sequence<256>>
struct AllTagRules_h {};
template<size_t ...I>
struct AllTagRules_h<std::index_sequence<I...>> {
	using type = std::tuple<RuleByTag_t<I>...>;
};
} // namespace details {

using AllTagRules_t = typename details::AllTagRules_h<>::type;

} // namespace mpp {
