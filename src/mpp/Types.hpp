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

namespace mpp {

using std::integral_constant;
using tnt::CStr;

/**
 * Specificators a wrappers around some objects, usually holds a constant
 * reference on that object and describes how that object must be packed to
 * msgpack stream.
 * For example std::tuple is packed as array by default, if you want it to
 * be packed as map, wrap it with mpp::as_map(<that tuple>).
 * Because of holding reference to an object a user must think about original
 * object's lifetime. The best practice is to use temporary specificator
 * objects, liks encoder.add(mpp::as_map(<that tuple>)).
 *
 * The short list of types and specificators in this header.
 * range - a set of values by two iterators, one iterator and size etc.
 * as_str - treat a value as string.
 * as_bin - treat a value as binary data.
 * as_arr - treat a value as msgpack array.
 * as_map - treat a value as msgpack map.
 * as_ext - treat a value as msgpack ext of given type.
 * as_raw - write already packed msgpack object as raw data.
 * reserve - skip some bytes and leave some space in the stream.
 * track - additionally save beginning and end positions of the written object.
 * as_fixed - fix underlying type of msgpack object.
 * MPP_AS_CONST - compile-time constant value (except strings).
 * MPP_AS_CONSTR - compile-time constant string value.
 */

/**
 * Simple helper for advancing standard iterators.
 */
template <class ITR>
ITR advance(ITR& itr, size_t n) { auto r = itr; std::advance(r, n); return r; }

/**
 * Range is a pair of iterators (in general meaning, pointers for example)
 * that has access methods as STL containers.
 * Should be constructed only with range(..) methods by a pair
 * of iterators or pointer and size. The size can be also passed as template
 * parameter thus declaring a range of fixed size.
 * Actual results will be one of the derivatives of Range class that will have
 * the following bool static constexpr:
 * is_fixed_size_v: the `size` method is static constexpr.
 * is_contiguous_v: the object also has standard `data` method.
 */
template <class ITR1, class ITR2>
struct RangeBase {
	ITR1 m_begin;
	ITR2 m_end;
	ITR1 begin() const { return m_begin; }
	const ITR2& end() const { return m_end; }
	size_t size() const { return std::distance(m_begin, m_end); }
};

template <class ITR1, class ITR2, bool IS_FIXED_SIZE, size_t N>
struct IteratorRange : RangeBase<ITR1, ITR2> {
	static constexpr bool is_fixed_size_v = true;
	static constexpr bool is_contiguous_v = false;
	static constexpr size_t size() { return N; }
};

template <class ITR1, class ITR2>
struct IteratorRange<ITR1, ITR2, false, 0> : RangeBase<ITR1, ITR2> {
	static constexpr bool is_fixed_size_v = false;
	static constexpr bool is_contiguous_v = false;
};

template <class ITR1, class ITR2, bool IS_FIXED_SIZE, size_t N>
struct ContiguousRange : IteratorRange<ITR1, ITR2, IS_FIXED_SIZE, N> {
	using IteratorRange<ITR1, ITR2, IS_FIXED_SIZE, N>::m_begin;
	static constexpr bool is_contiguous_v = true;
	auto data() const { return &*m_begin; }
};

template <class T>
constexpr IteratorRange<const T&, const T&, false, 0>
range(const T& begin, const T& end) { return {begin, end}; }

template <class T>
constexpr ContiguousRange<T*, T*, false, 0>
range(T* begin, T* end) { return {begin, end}; }

template <class T>
constexpr IteratorRange<const T&, T, false, 0>
range(const T& begin, size_t n) { return {begin, advance(begin, n)}; }

template <class T>
constexpr ContiguousRange<T*, T*, false, 0>
range(T* begin, size_t n) { return {begin, begin + n}; }

template <size_t N, class T>
constexpr IteratorRange<const T&, T, true, N>
range(const T& begin) { return {begin, advance(begin, N)}; }

template <size_t N, class T>
constexpr ContiguousRange<T*, T*, true, N>
range(T* begin) { return {begin, begin + N}; }

/**
 * A group of specificators - as_str(..), as_bin(..), as_arr(..), as_map(..),
 * as_raw(..).
 * They create wrappers str_holder, bin_holder etc respectively.
 * A wrapper takes a container or a range and specify explicitly how it must
 * be packed/unpacked as msgpack object.
 * A bit outstanding is as_raw - it means that the data passed is expected
 * to be a valid msgpack object and must be just copied to the stream.
 * Specificators also accept the same arguments as range(..), in that case
 * it's a synonym of as_xxx(range(...)).
 */
#define DEFINE_ARRLIKE_WRAPPER(name)						\
template <class T>								\
struct name##_holder {								\
	using type = T;								\
	const T& value;								\
};										\
										\
template <class... T>								\
constexpr auto as_##name(const T&... t)						\
{										\
	if constexpr (sizeof ... (T) == 1) {					\
		return name##_holder<T...>{t...};				\
	} else {								\
		using range_t = decltype(range(t...));				\
		return name##_holder<range_t>(range(t...));			\
	}									\
}										\
										\
template <size_t N, class... T>							\
constexpr auto as_##name(const T&... t)						\
{										\
	using range_t = decltype(range<N>(t...));				\
	return name##_holder<range_t>(range<N>(t...));				\
}										\
										\
struct forgot_to_add_semicolon

DEFINE_ARRLIKE_WRAPPER(str);
DEFINE_ARRLIKE_WRAPPER(bin);
DEFINE_ARRLIKE_WRAPPER(arr);
DEFINE_ARRLIKE_WRAPPER(map);
DEFINE_ARRLIKE_WRAPPER(raw);

#undef DEFINE_ARRLIKE_WRAPPER

/**
 * Specificator - as_ext(..). Creates a wrapper ext_holder that holds ext type
 * and a container (or range) and specifies that the data must packed/unpacked
 * as MP_EXIT msgpack object.
 * Specificator also accepts the same arguments as range(..), in that case
 * it's a synonym of as_ext(type, range(...)).
 */
template <class T>
struct ext_holder {
	using type = T;
	uint8_t ext_type;
	const T& value;
};

template <class... T>
auto as_ext(uint8_t type, T&&... t)
{
	if constexpr (sizeof...(T) == 1) {
		return ext_holder<T...>{type, t...};
	} else {
		using range_t = decltype(range(std::forward<T>(t)...));
		return ext_holder<range_t>{type, range(std::forward<T>(t)...)};
	}
}

template <size_t N, class... T>
auto as_ext(uint8_t type, T&&... t)
{
	using range_t = decltype(range<N>(std::forward<T>(t)...));
	return ext_holder<range_t>{type, range<N>(std::forward<T>(t)...)};
}

/**
 * Specificator - track(..). Creates a wrapper track_holder that holds a value
 * and a range - a pair of iterators. The first iterator will be set to the
 * beginning of written msgpack object, the second - at the end of it.
*/
template <class T, class RANGE>
struct track_holder {
	using type = T;
	const T& value;
	RANGE& range;
};

template <class T, class RANGE>
track_holder<T, RANGE> track(const T& t, RANGE& r) { return {t, r}; }

/**
 * Reserve is an object that specifies that some number of bytes must be skipped
 * (not written) in msgpack stream.
 * Should be created by reserve<N>() and reserve(N).
 * There are also reserve<N>(range) and reserve(N, range) specificators,
 * that are synonyms for track(reserve<N>, range) and track(reserve(N), range).
 */
template <size_t N>
struct Reserve {
	static constexpr bool is_const_size_v = true;
	static constexpr size_t value = N;
};

template <>
struct Reserve<0> {
	static constexpr bool is_const_size_v = false;
	size_t value;
};

template <size_t N>
Reserve<N> reserve() { return {}; }

Reserve<0> reserve(size_t n) { return {n}; }

template <size_t N, class RANGE>
track_holder<Reserve<N>, RANGE> reserve(RANGE& r) { return {{}, r}; }

template <class RANGE>
track_holder<Reserve<0>, RANGE> reserve(size_t n, RANGE& r) { return {{n}, r}; }

/**
 * Specificator - is_fixed(..). Creates a wrapper fixed_holder that holds
 * a value and a definite underlying type by which the value must be written
 * to msgpack stream.
 * For example: as_fixed<uint8_t>(1) will be packed as "0xcc0x01",
 * as_fixed<uint64_t>(1) will be packed as "0xcf0x00x00x00x00x00x00x000x01",
 * as_fixed<void>(1) will be packed as "0x01".
 * By default the type is determined by the type of given value, but it also
 * may be specified explicitly.
 * The 'void' type means that a value must be one-byte packed into msgpack tag.
 */
template <class T, class U>
struct fixed_holder {
	using type = T;
	using hold_type = U;
	const U& value;
};

template <class T, class U>
fixed_holder<T, U> as_fixed(const U& u) { return {u}; }

template <class T>
fixed_holder<T, T> as_fixed(const T& t) { return {t}; }

/**
 * Constants are types that have a constant value enclosed in type itself,
 * as some constexpr static member of the class.
 * For the most of constants std::integral_constant works perfectly.
 * MPP_AS_CONST is just a short form of creating an integral_constant.
 * There' some complexity in creating string constants. There's a special class
 * for them - CStr, that could be instantiated in a pair of ways.
 * MPP_AS_CONSTR is just a macro that instantiates on one of those forms.
 * There are also 'as_const' and 'as_constr' macros that are disabled by
 * default.
 */
#ifndef MPP_DISABLE_AS_CONST_MACRO
#define MPP_AS_CONST(x) std::integral_constant<decltype(x), x>{}
#endif
#ifndef TNT_DISABLE_STR_MACRO
#define MPP_AS_CONSTR(x) TNT_CON_STR(x)
#else
#ifndef TNT_DISABLE_STR_LITERAL
#define MPP_AS_CONSTR(x) x##_cs
#endif
#endif

#ifdef MPP_USE_SHORT_CONST_MACROS
#define as_const(x) std::integral_constant<decltype(x), x>{}
#define as_constr(x) MPP_AS_CONSTR(x)
#endif // #ifdef MPP_USE_SHORT_CONST_MACROS

}; // namespace mpp {
