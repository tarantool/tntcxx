#pragma once
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

#include <cstdint>
#include <type_traits>

namespace mpp {

/**
 * Getter of unsigned integer type with the size as given type.
 */
template <class T, class _ = void>
struct under_uint {  };

template <class T>
struct under_uint<T, std::enable_if_t<sizeof(T) == 1, void>> { using type = uint8_t; };
template <class T>
struct under_uint<T, std::enable_if_t<sizeof(T) == 2, void>> { using type = uint16_t; };
template <class T>
struct under_uint<T, std::enable_if_t<sizeof(T) == 4, void>> { using type = uint32_t; };
template <class T>
struct under_uint<T, std::enable_if_t<sizeof(T) == 8, void>> { using type = uint64_t; };

template <class T>
using under_uint_t = typename under_uint<T>::type;

/**
 * Getter of signed integer type with the size as given type.
 */
template <class T, class _ = void>
struct under_int {  };

template <class T>
struct under_int<T, std::enable_if_t<sizeof(T) == 1, void>> { using type = int8_t; };
template <class T>
struct under_int<T, std::enable_if_t<sizeof(T) == 2, void>> { using type = int16_t; };
template <class T>
struct under_int<T, std::enable_if_t<sizeof(T) == 4, void>> { using type = int32_t; };
template <class T>
struct under_int<T, std::enable_if_t<sizeof(T) == 8, void>> { using type = int64_t; };

template <class T>
using under_int_t = typename under_int<T>::type;

/**
 * bswap overloads.
 */
inline uint8_t  bswap(uint8_t x)  { return x; }
inline uint16_t bswap(uint16_t x) { return __builtin_bswap16(x); }
inline uint32_t bswap(uint32_t x) { return __builtin_bswap32(x); }
inline uint64_t bswap(uint64_t x) { return __builtin_bswap64(x); }

} // namespace mpp {