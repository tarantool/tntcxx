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
#include <limits>
#include <tuple>
#include <type_traits>

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
 * | Tag |   Suffix    | Ext |      Data      | Child0 | Child1 ... |
 * | <-----Value-----> |
 * | <-----------------Item-----------------> |
 * | <---------------------------Object---------------------------> |
 * Tag is the first byte of msgpack encoded object, completely describes type
 *  of the object and layout of item. In many cases also contains the value,
 *  in those cases the next `suffix` part is omitted.
 * Value - numeric part of object. Is the length of data for str/bin/ext,
 *  the length of array for arr, number k-v parts for map, and the value itself
 *  for int/bool/float etc.
 * Suffix - big-endian bytes of value if the value is not encoded in tag;
 *  can be of size 0,1,2,4,8, which is fully specified by preceding tag.
 * Ext - additional byte that is present only for msgpack extension family
 *  and describes the type of that extension.
 * Data - byte sequence for str/bin/ext (absent for other families);
 *  the size of data (if it is present!) is equal to preceding value.
 * Item - unit of msgpack stream, the encoded object without children. The
 *  size of item is specified by tag and value.
 * Child0... - msgpack independently encoded elements of array or keys-values
 *  of maps. The number of direct children is preceding value of an array of
 *  2 * preceding value of a map.
 *
 * The important part of msgpack format is that generally the value can be
 * encoded right in tag OR the tag specifies the number of multi bytes nearby,
 * in which the value is encoded. That gives two ways of value encoding, that
 * are called in this file as Simplex (value is encoded in one byte tag) and
 * Complex (value is encoded in multi bytes with a preceding tag). Not every
 * family can be encoded in both ways, for example MP_BOOL can be only simplex
 * encoded, while MP_BIN can only be complex encoded.
 *
 * Each rule class has a base - BaseRule and thus its members (see the class
 * definition for the list of members). In addition the rule may or may not
 * have Simplex and Complex encoding definition. Absence of one or another
 * part declares that the family cannon be encoded in the corresponding way.
 * Simplex part:
 * simplex_value_range - range (including boundaries) of values that
 *  can be encoded in simplex way. Note that the ranges can have negative
 *  boundaries.
 * simplex_tag - the tag by which the first value in the first range is
 *  encoded. The rest values take incrementally sequent tag.
 * Complex part:
 * complex_types - std::tuple with types that are actually put to the
 *  multibyte part.
 * complex_tag - the tag that stands for the first type in complex_types;
 *  the following types take incrementally sequent tag.
 */

namespace mpp {

template <class T>
struct RuleRange {
	T first;
	T last;
	size_t count;
	constexpr RuleRange(T first_, T last_) : first(first_), last(last_),
		count(static_cast<size_t>(last - first + 1)) {}
};

template <compact::Family FAMILY, class ...TYPE>
struct BaseRule {
	// Widest types that can represent the value.
	using types = std::tuple<TYPE...>;
	// Msgpack family.
	static constexpr compact::Family family = FAMILY;
	// The rule stores bool values.
	static constexpr bool is_bool = FAMILY == compact::MP_BOOL;
	// The rule stores floating point values.
	static constexpr bool is_floating_point = FAMILY == compact::MP_FLT;
	// The rule actually doest not store value, only type matters.
	static constexpr bool is_valueless = FAMILY == compact::MP_NIL ||
					     FAMILY == compact::MP_IGNR;
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
		FAMILY == compact::MP_ARR ? 1 :
		FAMILY == compact::MP_MAP ? 2 : 0;
	// The rule has simplex form.
	static constexpr bool has_simplex = FAMILY == compact::MP_NIL ||
					    FAMILY == compact::MP_IGNR ||
					    FAMILY == compact::MP_BOOL ||
					    FAMILY == compact::MP_INT ||
					    FAMILY == compact::MP_STR ||
					    FAMILY == compact::MP_ARR ||
					    FAMILY == compact::MP_MAP ||
					    FAMILY == compact::MP_EXT;
	// The rule has simplex form.
	static constexpr bool has_complex = FAMILY == compact::MP_INT ||
					    FAMILY == compact::MP_FLT ||
					    FAMILY == compact::MP_STR ||
					    FAMILY == compact::MP_BIN ||
					    FAMILY == compact::MP_ARR ||
					    FAMILY == compact::MP_MAP ||
					    FAMILY == compact::MP_EXT;
	// The rule has signed simplex range.
	static constexpr bool is_simplex_signed = FAMILY == compact::MP_INT;
	// The rule has logarithmic simplex range.
	static constexpr bool is_simplex_log_range = FAMILY == compact::MP_EXT;
	// The type of simplex value in simplex range.
	using simplex_value_t =
		std::conditional_t<is_simplex_signed, int8_t, uint8_t>;
	// The type of simplex range.
	using simplex_value_range_t = RuleRange<simplex_value_t>;
};

struct NilRule : BaseRule<compact::MP_NIL, std::nullptr_t> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 0};
	static constexpr uint8_t simplex_tag = 0xc0;
};

struct IgnrRule : BaseRule<compact::MP_IGNR, decltype(std::ignore)> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 0};
	static constexpr uint8_t simplex_tag = 0xc1;
};

struct BoolRule : BaseRule<compact::MP_BOOL, bool> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 1};
	static constexpr uint8_t simplex_tag = 0xc2;
};

struct IntRule : BaseRule<compact::MP_INT, uint64_t, int64_t> {
	static constexpr simplex_value_range_t simplex_value_range = {-32, 127};
	static constexpr uint8_t simplex_tag = 0x00;
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t, uint64_t,
		int8_t, int16_t, int32_t, int64_t>;
	static constexpr uint8_t complex_tag = 0xcc;
};

struct FltRule : BaseRule<compact::MP_FLT, double> {
	using complex_types = std::tuple<float, double>;
	static constexpr uint8_t complex_tag = 0xca;
};

struct StrRule : BaseRule<compact::MP_STR, uint32_t> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 31};
	static constexpr uint8_t simplex_tag = 0xa0;
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xd9;
};

struct BinRule : BaseRule<compact::MP_BIN, uint32_t> {
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xc4;
};

struct ArrRule : BaseRule<compact::MP_ARR, uint32_t> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 15};
	static constexpr uint8_t simplex_tag = 0x90;
	using complex_types = std::tuple<uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xdc;
};

struct MapRule : BaseRule<compact::MP_MAP, uint32_t> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 15};
	static constexpr uint8_t simplex_tag = 0x80;
	using complex_types = std::tuple<uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xde;
};

struct ExtRule : BaseRule<compact::MP_EXT, uint32_t> {
	static constexpr simplex_value_range_t simplex_value_range = {0, 4};
	static constexpr uint8_t simplex_tag = 0xd4;
	using complex_types = std::tuple<uint8_t, uint16_t, uint32_t>;
	static constexpr uint8_t complex_tag = 0xc7;
};

using all_rules_t = std::tuple<NilRule, IgnrRule, BoolRule, IntRule,
	FltRule, StrRule, BinRule, ArrRule, MapRule, ExtRule>;

/**
 * Find a rule by compact::Family.
 */
template <compact::Family FAMILY>
using rule_by_family_t = std::tuple_element_t<FAMILY, all_rules_t>;

/**
 * Helper of rule_complex_types_safe_t.
 */
namespace details {
template <class RULE, bool HAS_COMPLEX = RULE::has_complex>
struct rule_complex_types_safe_h {
	using type = std::tuple<>;
};

template <class RULE>
struct rule_complex_types_safe_h<RULE, true> {
	using type = typename RULE::complex_types;
};
} // namespace details

/**
 * Get RULE::complex_types if present, or empty std::tuple otherwise.
 */
template <class RULE>
using rule_complex_types_safe_t =
	typename details::rule_complex_types_safe_h<RULE>::type;

/**
 * Get number of complex variants (0 if none).
 */
template <class RULE>
constexpr uint8_t rule_complex_count_v =
	std::tuple_size_v<rule_complex_types_safe_t<RULE>>;

/**
 * Range of tags by which a value can be encoded in simplex forms.
 * Warning: for MP_INT this range comes with int8_t boundaries.
 */
template <class RULE>
constexpr RuleRange<typename RULE::simplex_value_t> rule_simplex_tag_range_v =
	{RULE::simplex_value_range.first + RULE::simplex_tag,
	 RULE::simplex_value_range.last + RULE::simplex_tag};

/**
 * Getter of a range of tags by which a value can be encoded in complex forms.
 */
template <class RULE>
constexpr RuleRange<uint8_t> rule_complex_tag_range_v =
	{RULE::complex_tag,
	 RULE::complex_tag + (rule_complex_count_v<RULE> - 1)};

namespace details {
/**
 * Helper of find_simplex_offset: convert MP_EXT value to simplex tag offset.
 */
constexpr size_t exp_range[] =
	{5, 0, 1, 5, 2, 5, 5, 5, 3, 5, 5, 5, 5, 5, 5, 5, 4};

/**
 * Helper of find_simplex_offset: convert enum type to underlying type.
 */
template <class T>
constexpr auto rule_unenum(T t) {
	if constexpr (std::is_enum_v<T>)
		return static_cast<std::underlying_type_t<T>>(t);
	else
		return t;
}
} // namespace details

/**
 * Try to find a simplex offset of a value in a rule.
 * If failed, RULE::simplex_value_range.count is returned.
 * Otherwise that value can be encoded with a tag RULE::simplex_tag + result.
 */
template <class RULE, class E>
constexpr int find_simplex_offset([[maybe_unused]] E eval)
{
	static_assert(RULE::has_simplex);
	static_assert(!RULE::is_floating_point);
	auto val = details::rule_unenum(eval);
	using V = decltype(val);
	if constexpr (RULE::is_bool) {
		return !!val;
	} else if constexpr (RULE::is_valueless) {
		return 0;
	} else if constexpr (RULE::is_simplex_log_range) {
		if (val > (1 << RULE::simplex_value_range.last))
			return RULE::simplex_value_range.count;
		static_assert(std::size(details::exp_range) ==
			      1 + (1 << RULE::simplex_value_range.last));
		static_assert(details::exp_range[0] ==
			      RULE::simplex_value_range.count);
		return static_cast<int>(details::exp_range[val]);
	} else if constexpr (!RULE::is_simplex_signed || !std::is_signed_v<V>) {
		constexpr std::make_unsigned_t<typename RULE::simplex_value_t>
		        range_last = RULE::simplex_value_range.last;
		if (val <= range_last)
			return static_cast<int>(val);
		else
			return RULE::simplex_value_range.count;
	} else {
		if (val >= RULE::simplex_value_range.first &&
		    val <= RULE::simplex_value_range.last)
			return static_cast<int>(val);
		else
			return RULE::simplex_value_range.count;
	}
}

namespace details {
/**
 * Helper of rule_complex_apply, see description below.
 */
template <class RULE, size_t I, size_t N, class V, class F>
constexpr size_t rule_complex_apply_h([[maybe_unused]] V val, F &&f)
{
	using arg_t = std::integral_constant<size_t, I>;
	if constexpr(I + 1 < N) {
		using types = typename RULE::complex_types;
		using type = std::tuple_element_t<I, types>;
		using lim = std::numeric_limits<type>;
		if constexpr (RULE::is_floating_point) {
			static_assert(std::is_floating_point_v<type>);
			if (std::is_floating_point_v<V>) {
				if constexpr(sizeof(V) <= sizeof(type))
					return f(arg_t{});
			} else {
				if constexpr(sizeof(V) < sizeof(type))
					return f(arg_t{});
			}
		} else if constexpr (std::is_signed_v<type>) {
			// V must be negative here.
			static_assert(std::is_signed_v<V>);
			if constexpr(sizeof(V) <= sizeof(type)) {
				return f(arg_t{});
			} else {
				if (val >= lim::min())
					return f(arg_t{});
			}
		} else {
			if constexpr(sizeof(V) <= sizeof(type)) {
				return f(arg_t{});
			} else {
				if (val <= lim::max())
					return f(arg_t{});
			}
		}
		return rule_complex_apply_h<RULE, I + 1, N>(val,
							    std::forward<F>(f));
	} else {
		return f(arg_t{});
	}
}
} // namespace details

/**
 * Find a complex type in @a RULE that fits value @a eval most, and call
 * given functor @a f with an argument of std::integral_constant<size_t, I>,
 * where I is the offset of that complex type.
 * Return the functor's result.
 */
template <class RULE, class E, class F>
constexpr size_t rule_complex_apply(E eval, F &&f)
{
	static_assert(RULE::has_complex);
	auto val = details::rule_unenum(eval);
	using V = decltype(val);
	constexpr bool is_int = RULE::family == compact::MP_INT;
	constexpr size_t N = rule_complex_count_v<RULE>;
	if constexpr (is_int && std::is_signed_v<V>) {
		static_assert(N == 8);
		if (val < 0) {
			return details::rule_complex_apply_h<RULE, 4, 8>(val,
				std::forward<F>(f));
		} else {
			using U = std::make_unsigned_t<V>;
			auto u = static_cast<U>(val);
			return details::rule_complex_apply_h<RULE, 0, 4>(u,
				std::forward<F>(f));
		}
	} else if constexpr (is_int && !std::is_signed_v<V>) {
		static_assert(N == 8);
		return details::rule_complex_apply_h<RULE, 0, 4>(val,
			std::forward<F>(f));
	} else {
		return details::rule_complex_apply_h<RULE, 0, N>(val,
			std::forward<F>(f));
	}
}

/**
 * Find an offset of complex tag by given value.
 * That means that value can be encoded with a tag RULE::complex_tag + result.
 */
template <class RULE, class V>
constexpr size_t find_complex_offset(V val)
{
	return rule_complex_apply<RULE>(val, [&](auto IND) -> size_t {
		return decltype(IND)::value;
	});
}

} // namespace mpp {
