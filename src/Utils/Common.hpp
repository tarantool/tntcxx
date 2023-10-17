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

/**
 * Several generic utilities. Short list (see full description below):
 * always_false_v<T>
 * unreachable()
 * tuple_cut_t<T> (remove the first element from Tuple)
 * first_t<T>
 * last_t<T>
 * tuple_find_v<X, T> (index of type with Size in Tuple)
 * tuple_find_size_v<S, T> (index of type with Size in Tuple)
 * uint_types (tuple with unsigned ints)
 * int_types (tuple with signed ints)
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace tnt {
/**
 * Delayer of static_assert evaluation.
 */
template <class>
constexpr bool always_false_v = false;

/**
 * Standard unreachable.
 */
[[noreturn]] inline void unreachable() { assert(false); __builtin_unreachable(); }

/**
 * Remove the first element from tuple
 */
template <class T>
struct tuple_cut { static_assert(always_false_v<T>, "Wrong usage!"); };

template <class T, class... U>
struct tuple_cut<std::tuple<T, U...>> { using type = std::tuple<U...>; };

template <class T>
using tuple_cut_t = typename tuple_cut<T>::type;

/**
 * First and last types of a tuple.
 */
template <class TUPLE>
using first_t = std::tuple_element_t<0, TUPLE>;

template <class TUPLE>
using last_t = std::tuple_element_t<std::tuple_size_v<TUPLE> - 1, TUPLE>;

/**
 * Find an index in tuple of a given type.
 */
template <class X, class TUPLE>
struct tuple_find;

template <class X>
struct tuple_find<X, std::tuple<>> { static constexpr size_t value = 0; };

template <class X, class TUPLE>
struct tuple_find {
	static constexpr size_t value =
		std::is_same_v<X, std::tuple_element_t<0, TUPLE>> ?
		0 : 1 + tuple_find<X, tuple_cut_t<TUPLE>>::value;
};

template <class X, class TUPLE>
constexpr size_t tuple_find_v = tuple_find<X, TUPLE>::value;

/**
 * Find an index in tuple of a type which has given size.
 */
template <size_t S, class TUPLE>
struct tuple_find_size;

template <size_t S>
struct tuple_find_size<S, std::tuple<>> { static constexpr size_t value = 0; };

template <size_t S, class TUPLE>
struct tuple_find_size {
	static constexpr size_t value =
		sizeof(std::tuple_element_t<0, TUPLE>) == S ?
		0 : 1 + tuple_find_size<S, tuple_cut_t<TUPLE>>::value;
};

template <size_t S, class TUPLE>
constexpr size_t tuple_find_size_v = tuple_find_size<S, TUPLE>::value;

/**
 * All standard uint and int types.
 */
using uint_types = std::tuple<uint8_t, uint16_t, uint32_t, uint64_t>;
using int_types = std::tuple<int8_t, int16_t, int32_t, int64_t>;

} // namespace mpp {
