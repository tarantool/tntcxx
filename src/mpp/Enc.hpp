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

#include <cstdint>
#include <cstring>
#include <iterator>
#include <tuple>
#include <variant>

#include "BSwap.hpp"
#include "Constants.hpp"
#include "Types.hpp"
#include "Traits.hpp"
#include "../Utils/Common.hpp"

//TODO : add std::variant
//TODO : add time_t?
//TODO : rollback in case of fail

namespace mpp {

template <class BUFFER>
class Enc
{
	using Buffer_t = BUFFER;
	using iterator_t = typename BUFFER::iterator;

public:
	using iterator = iterator_t;
	struct range
	{
		iterator_t first, second;
		range(Buffer_t& buf) : first(buf), second(buf) {}
		void unlink() { first.unlink(); second.unlink(); }
	};
	struct save_iterator
	{
		iterator_t &itr;
	};

	explicit Enc(Buffer_t& buf) : m_Buf(buf) {}


	BUFFER& getBuf() { return m_Buf; }

	template <class... T>
	void add(const T&... t)
	{
		add_internal<compact::MP_END, false, void>(CStr<>(), t...);
	}

private:
	template <bool V>
	static constexpr auto conv_const_bool();
	template <uint64_t V>
	static constexpr auto conv_const_uint();
	template <int64_t V>
	static constexpr auto conv_const_int();
	template <uint32_t V>
	static constexpr auto conv_const_str();
	template <uint32_t V>
	static constexpr auto conv_const_bin();
	template <uint32_t V>
	static constexpr auto conv_const_arr();
	template <uint32_t V>
	static constexpr auto conv_const_map();

	template <char...C, class T>
	void add_int(CStr<C...> prefix, T t);
	template <char...C, class T>
	void add_flt(CStr<C...> prefix, T t);
	template <char...C, class T>
	void add_str(CStr<C...> prefix, T size);
	template <char...C, class T>
	void add_bin(CStr<C...> prefix, T size);
	template <char...C, class T>
	void add_arr(CStr<C...> prefix, T size);
	template <char...C, class T>
	void add_map(CStr<C...> prefix, T size);

	template <compact::Family TYPE, bool FIXED_SET, class FIXED_TYPE,
		  char... C, class T, class... MORE>
	void add_internal(CStr<C...> prefix, const T& t, const MORE&... more);

	template <compact::Family, bool, class, char... C>
	void add_internal(CStr<C...> suffix);

	BUFFER& m_Buf;
};

template <class T, T V, size_t... I>
constexpr auto const_bswap_helper(std::index_sequence<I...>)
{
	return CStr<((V >> (8 * (sizeof...(I) - I - 1))) & 0xff)...>{};
}

template <class T, T V>
constexpr auto const_bswap()
{
	return const_bswap_helper<std::make_unsigned_t<T>,
		static_cast<std::make_unsigned_t<T>>(V)>(
		std::make_index_sequence<sizeof(T)>{});
}

template <class T>
under_int_t<T> enc_bswap(T t)
{
	under_uint_t<T> tmp;
	memcpy(&tmp, &t, sizeof(T));
	return bswap(tmp);
}

template <class BUFFER>
template <bool V>
constexpr auto
Enc<BUFFER>::conv_const_bool()
{
	return CStr<V ? 0xc2 : 0xc3>{};
}

template <class BUFFER>
template <uint64_t V>
constexpr auto
Enc<BUFFER>::conv_const_uint()
{
	if constexpr (V <= 127)
		return CStr<V>{};
	else if constexpr (V <= UINT8_MAX)
		return CStr<'\xcc'>{}.join(const_bswap<uint8_t, V>);
	else if constexpr (V <= UINT16_MAX)
		return CStr<'\xcd'>{}.join(const_bswap<uint16_t, V>);
	else if constexpr (V <= UINT32_MAX)
		return CStr<'\xce'>{}.join(const_bswap<uint32_t, V>);
	else
		return CStr<'\xcf'>{}.join(const_bswap<uint64_t, V>);
}

template <class BUFFER>
template <int64_t V>
constexpr auto
Enc<BUFFER>::conv_const_int()
{
	if constexpr (V >= 0)
		return conv_const_uint<V>();
	if constexpr (V >= -32)
		return CStr<V>{};
	else if constexpr (V >= INT8_MIN)
		return CStr<'\xd0'>{}.join(const_bswap<int8_t, V>);
	else if constexpr (V >= INT16_MIN)
		return CStr<'\xd1'>{}.join(const_bswap<int16_t, V>);
	else if constexpr (V >= INT32_MIN)
		return CStr<'\xd2'>{}.join(const_bswap<int32_t, V>);
	else
		return CStr<'\xd3'>{}.join(const_bswap<int64_t, V>);
}

template <class BUFFER>
template <uint32_t V>
constexpr auto
Enc<BUFFER>::conv_const_str()
{
	if constexpr (V < 32)
		return CStr<'\xa0' + V>{};
	else if constexpr (V < UINT8_MAX)
		return CStr<'\xd9'>{}.join(const_bswap<uint8_t, V>);
	else if constexpr (V < UINT16_MAX)
		return CStr<'\xda'>{}.join(const_bswap<uint16_t, V>);
	else
		return CStr<'\xdb'>{}.join(const_bswap<uint32_t, V>);
}

template <class BUFFER>
template <uint32_t V>
constexpr auto
Enc<BUFFER>::conv_const_bin()
{
	if constexpr (V < UINT8_MAX)
		return CStr<'\xc4'>{}.join(const_bswap<uint8_t, V>);
	else if constexpr (V < UINT16_MAX)
		return CStr<'\xc5'>{}.join(const_bswap<uint16_t, V>);
	else
		return CStr<'\xc6'>{}.join(const_bswap<uint32_t, V>);
}

template <class BUFFER>
template <uint32_t V>
constexpr auto
Enc<BUFFER>::conv_const_arr()
{
	if constexpr (V < 16)
		return CStr<'\x90' + V>{};
	else if constexpr (V < UINT16_MAX)
		return CStr<'\xdc'>{}.join(const_bswap<uint16_t, V>);
	else
		return CStr<'\xdd'>{}.join(const_bswap<uint32_t, V>);
}

template <class BUFFER>
template <uint32_t V>
constexpr auto
Enc<BUFFER>::conv_const_map()
{
	if constexpr (V < 16)
		return CStr<'\x80' + V>{};
	else if constexpr (V < UINT16_MAX)
		return CStr<'\xde'>{}.join(const_bswap<uint16_t, V>);
	else
		return CStr<'\xdf'>{}.join(const_bswap<uint32_t, V>);
}

template <class BUFFER>
template <char...C, class T>
void
Enc<BUFFER>::add_int(CStr<C...> prefix, T t)
{
	static_assert(std::is_integral_v<T>);

	if constexpr (std::is_signed_v<T>) if (t < 0) {
		if (t >= -32) {
			m_Buf.addBack(prefix);
			char c = static_cast<char>(t);
			m_Buf.addBack(c);
			return;
		}
		if constexpr (sizeof(T) > 4) if (t < INT32_MIN) {
				auto add = CStr<'\xd3'>{};
				m_Buf.addBack(prefix.join(add));
				m_Buf.addBack(enc_bswap(static_cast<int64_t>(t)));
				return;
			}
		if constexpr (sizeof(T) > 2) if (t < INT16_MIN) {
				auto add = CStr<'\xd2'>{};
				m_Buf.addBack(prefix.join(add));
				m_Buf.addBack(enc_bswap(static_cast<int32_t>(t)));
				return;
			}
		if constexpr (sizeof(T) > 1) if (t < INT8_MIN) {
				auto add = CStr<'\xd1'>{};
				m_Buf.addBack(prefix.join(add));
				m_Buf.addBack(enc_bswap(static_cast<int16_t>(t)));
				return;
			}
		auto add = CStr<'\xd0'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<int8_t>(t)));
		return;
	}

	if (t <= 127) {
		m_Buf.addBack(prefix);
		char c = static_cast<char>(t);
		m_Buf.addBack(c);
		return;
	}
	if constexpr (sizeof(T) > 4) if (t > UINT32_MAX) {
		auto add = CStr<'\xcf'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint64_t>(t)));
		return;
	}
	if constexpr (sizeof(T) > 2) if (t > UINT16_MAX) {
		auto add = CStr<'\xce'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint32_t>(t)));
		return;
	}
	if constexpr (sizeof(T) > 1) if (t > UINT8_MAX) {
		auto add = CStr<'\xcd'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint16_t>(t)));
		return;
	}
	auto add = CStr<'\xcc'>{};
	m_Buf.addBack(prefix.join(add));
	m_Buf.addBack(enc_bswap(static_cast<uint8_t>(t)));
}

template <class BUFFER>
template <char...C, class T>
void
Enc<BUFFER>::add_flt(CStr<C...> prefix, T t)
{
	static_assert(std::is_floating_point_v<T>);
	static_assert(sizeof(T) & 12u, "Not a floating point");
	constexpr char tag = sizeof(T) == 4 ? '\xca' : '\xcb';
	auto add = CStr<tag>{};
	m_Buf.addBack(prefix.join(add));
	under_uint_t<T> tmp;
	memcpy(&tmp, &t, sizeof(T));
	m_Buf.addBack(enc_bswap(tmp));
}

template <class BUFFER>
template <char...C, class T>
void
Enc<BUFFER>::add_str(CStr<C...> prefix, T size)
{
	static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
	if constexpr (sizeof(T) > 4)
		assert(size <= UINT32_MAX);
	if (size < 32) {
		m_Buf.addBack(prefix);
		char c = '\xa0' + size;
		m_Buf.addBack(c);
		return;
	}
	if constexpr (sizeof(T) > 2) if (size > UINT16_MAX) {
		auto add = CStr<'\xdb'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint32_t>(size)));
		return;
	}
	if constexpr (sizeof(T) > 1) if (size > UINT8_MAX) {
		auto add = CStr<'\xda'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint16_t>(size)));
		return;
	}
	auto add = CStr<'\xd9'>{};
	m_Buf.addBack(prefix.join(add));
	m_Buf.addBack(enc_bswap(static_cast<uint8_t>(size)));
}

template <class BUFFER>
template <char...C, class T>
void
Enc<BUFFER>::add_bin(CStr<C...> prefix, T size)
{
	static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
	if constexpr (sizeof(T) > 4)
		assert(size <= UINT32_MAX);
	if constexpr (sizeof(T) > 2) if (size > UINT16_MAX) {
		auto add = CStr<'\xc6'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint32_t>(size)));
		return;
	}
	if constexpr (sizeof(T) > 1) if (size > UINT8_MAX) {
		auto add = CStr<'\xc5'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint16_t>(size)));
		return;
	}
	auto add = CStr<'\xc4'>{};
	m_Buf.addBack(prefix.join(add));
	m_Buf.addBack(enc_bswap(static_cast<uint8_t>(size)));
}

template <class BUFFER>
template <char...C, class T>
void
Enc<BUFFER>::add_arr(CStr<C...> prefix, T size)
{
	static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
	if constexpr (sizeof(T) > 4)
		assert(size <= UINT32_MAX);

	if (size < 16) {
		m_Buf.addBack(prefix);
		char c = '\x90' + size;
		m_Buf.addBack(c);
		return;
	}
	if constexpr (sizeof(T) > 2) if (size > UINT16_MAX) {
		auto add = CStr<'\xdd'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint32_t>(size)));
		return;
	}
	auto add = CStr<'\xdc'>{};
	m_Buf.addBack(prefix.join(add));
	m_Buf.addBack(enc_bswap(static_cast<uint16_t>(size)));
}

template <class BUFFER>
template <char...C, class T>
void
Enc<BUFFER>::add_map(CStr<C...> prefix, T size)
{
	static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
	if constexpr (sizeof(T) > 4)
		assert(size <= UINT32_MAX);

	if (size < 16) {
		m_Buf.addBack(prefix);
		char c = '\x80' + size;
		m_Buf.addBack(c);
		return;
	}
	if constexpr (sizeof(T) > 2) if (size > UINT16_MAX) {
		auto add = CStr<'\xdf'>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<uint32_t>(size)));
		return;
	}
	auto add = CStr<'\xde'>{};
	m_Buf.addBack(prefix.join(add));
	m_Buf.addBack(enc_bswap(static_cast<uint16_t>(size)));
}

template <class BUFFER>
template <compact::Family TYPE, bool FIXED_SET, class FIXED_TYPE, char... C>
void
Enc<BUFFER>::add_internal(CStr<C...> suffix)
{
	m_Buf.addBack(suffix);
}

template <class BUFFER>
template <compact::Family TYPE, bool FIXED_SET, class FIXED_TYPE,
	  char... C, class T, class... MORE>
void
Enc<BUFFER>::add_internal(CStr<C...> prefix, const T& t, const MORE&... more)
{
	if constexpr (FIXED_SET && (is_const_v<T> || is_constr_v<T>))
		static_assert(always_false_v<T>, "Why to fix a const?");
	if constexpr (is_const_v<T> && TYPE != compact::MP_END)
		static_assert(always_false_v<T>, "Const arith don't need type");

	if constexpr (is_simple_spec_v<T>) {
		static_assert(TYPE == compact::MP_END, "Double type spec?");
		add_internal<get_simple_family<T>(), FIXED_SET, FIXED_TYPE>(
			prefix, t.value, more...);
	} else if constexpr (is_fixed_v<T>) {
		using fix_type = typename T::type;
		add_internal<TYPE, true, fix_type>(prefix, t.value, more...);
	} else if constexpr (is_track_v<T>) {
		m_Buf.addBack(prefix);
		t.range.first = m_Buf.end();
		save_iterator save{t.range.second};
		add_internal<TYPE, FIXED_SET, FIXED_TYPE>(CStr<>{}, t.value,
							  save, more...);
	} else if constexpr (std::is_same_v<T, save_iterator>) {
		m_Buf.addBack(prefix);
		t.itr = m_Buf.end();
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (is_const_b<T>) {
		constexpr auto add = conv_const_bool<T::value>();
		add_internal<compact::MP_END, false, void>(prefix.join(add), more...);
	} else if constexpr (is_const_f<T> || is_const_d<T>) {
		// TODO: invent how to pack floats in compile time.
		add_internal<TYPE, FIXED_SET, FIXED_TYPE>(prefix, t.value, more...);
	} else if constexpr (is_const_e<T>) {
		if constexpr (T::value < 0) {
			constexpr under_int_t<T> v = static_cast<under_int_t<T>>(T::value);
			using u = std::integral_constant<under_int_t<T>, v>;
			add_internal<TYPE, FIXED_SET, FIXED_TYPE>(prefix, u{}, more...);
		} else {
			constexpr under_uint_t<T> v = static_cast<under_uint_t<T>>(T::value);
			using u = std::integral_constant<under_uint_t<T>, v>;
			add_internal<TYPE, FIXED_SET, FIXED_TYPE>(prefix, u{}, more...);
		}
	} else if constexpr (is_const_s<T>) {
		constexpr auto add = conv_const_int<T::value>();
		add_internal<compact::MP_END, false, void>(prefix.join(add), more...);
	} else if constexpr (is_const_u<T>) {
		constexpr auto add = conv_const_uint<T::value>();
		add_internal<compact::MP_END, false, void>(prefix.join(add), more...);
	} else if constexpr (is_const_v<T>) {
		static_assert(always_false_v<T>, "Unknown const!");
	} else if constexpr (is_constr_v<T> && TYPE == compact::MP_BIN) {
		constexpr auto add = conv_const_bin<T::size>().join(t);
		add_internal<compact::MP_END, false, void>(prefix.join(add), more...);
	} else if constexpr (is_constr_v<T>) {
		static_assert(TYPE == compact::MP_END || TYPE == compact::MP_STR,
			"What else can be packed as string?");
		constexpr auto add = conv_const_str<T::size>().join(t);
		add_internal<compact::MP_END, false, void>(prefix.join(add), more...);
	} else if constexpr (is_raw_v<T>) {
		m_Buf.addBack(prefix);
		m_Buf.addBack(std::data(t.value), std::size(t.value));
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (is_reserve_v<T>) {
		m_Buf.addBack(prefix);
		m_Buf.advanceBack(t.value);
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (std::is_same_v<T, std::nullptr_t>) {
		constexpr auto add = CStr<'\xc0'>{};
		add_internal<compact::MP_END, false, void>(prefix.join(add), more...);
	} else if constexpr (std::is_same_v<T, bool>) {
		m_Buf.addBack(prefix);
		char c = '\xc2' + t;
		m_Buf.addBack(c);
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);

	} else if constexpr (std::is_enum_v<T>) {
		if (t < 0) {
			under_int_t<T> u = static_cast<under_int_t<T>>(t);
			add_internal<TYPE, FIXED_SET, FIXED_TYPE>(prefix, u, more...);
		} else {
			under_uint_t<T> u = static_cast<under_uint_t<T>>(t);
			add_internal<TYPE, FIXED_SET, FIXED_TYPE>(prefix, u, more...);
		}
	} else if constexpr (std::is_integral_v<T> && FIXED_SET &&
			     std::is_same_v<FIXED_TYPE, void>) {
		m_Buf.addBack(prefix);
		char c = static_cast<char>(t);
		m_Buf.addBack(c);
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (std::is_integral_v<T> && FIXED_SET) {
		constexpr char tag_start = std::is_signed_v<T> ? '\xd0' : '\xcc';
		auto add = CStr<tag_start + power_v<FIXED_TYPE>()>{};
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(enc_bswap(static_cast<FIXED_TYPE>(t)));
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (std::is_integral_v<T>) {
		add_int(prefix, t);
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);

	} else if constexpr (std::is_floating_point_v<T>) {
		add_flt(prefix, t);
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);

	} else if constexpr (TYPE == compact::MP_STR && FIXED_SET &&
			     std::is_same_v<FIXED_TYPE, void>) {
		m_Buf.addBack(prefix);
		size_t sz;
		if constexpr(is_c_str_v<T>)
			sz = strlen(t);
		else
			sz = std::size(t);
		char c = '\xa0' + sz;
		m_Buf.addBack(c);
		if constexpr(is_c_str_v<T>)
			m_Buf.addBack(t, sz);
		else
			m_Buf.addBack(std::data(t), std::size(t));
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_STR && FIXED_SET) {
		constexpr char tag_start = '\xd9';
		auto add = CStr<tag_start + power_v<FIXED_TYPE>()>{};
		m_Buf.addBack(prefix.join(add));
		size_t sz;
		if constexpr(is_c_str_v<T>)
			sz = strlen(t);
		else
			sz = std::size(t);
		m_Buf.addBack(enc_bswap(static_cast<FIXED_TYPE>(sz)));
		if constexpr(is_c_str_v<T>)
			m_Buf.addBack(t, sz);
		else
			m_Buf.addBack(std::data(t), sz);
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_STR &&
			     has_fixed_size_v<T> && !std::is_array_v<T>) {
		// We excluded C style array of characters because string
		// literals are also arrays of characters including terminal
		// null character which is usually not expected to be encoded.
		auto add = conv_const_str<get_fixed_size_v<T>>();
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack(std::data(t), std::size(t));
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_STR) {
		if constexpr(is_c_str_v<T> || std::is_array_v<T>) {
			size_t size = strlen(t);
			add_str(prefix, size);
			m_Buf.addBack({t, size});
		} else {
			add_str(prefix, t.size());
			m_Buf.addBack({std::data(t), std::size(t)});
		}
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);

	} else if constexpr (TYPE == compact::MP_BIN && FIXED_SET) {
		static_assert(!std::is_same_v<FIXED_TYPE, void>,
			      "MP_BIN doesn't have one-tag encoding!");
		constexpr char tag_start = '\xc4';
		auto add = CStr<tag_start + power_v<FIXED_TYPE>()>{};
		m_Buf.addBack(prefix.join(add));
		size_t sz;
		if constexpr(is_c_str_v<T>)
			sz = strlen(t);
		else
			sz = std::size(t);
		m_Buf.addBack(enc_bswap(static_cast<FIXED_TYPE>(sz)));
		if constexpr(is_c_str_v<T>)
			m_Buf.addBack({t, sz});
		else
			m_Buf.addBack({std::data(t), sz});
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_BIN && has_fixed_size_v<T>) {
		auto add = conv_const_bin<get_fixed_size_v<T>>();
		m_Buf.addBack(prefix.join(add));
		m_Buf.addBack({std::data(t), std::size(t)});
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_BIN) {
		if constexpr(is_c_str_v<T>) {
			static_assert(always_false_v<T>, "C string as BIN?");
			size_t size = strlen(t);
			add_bin(prefix, size);
			m_Buf.addBack({t, size});
		} else {
			add_bin(prefix, t.size());
			m_Buf.addBack({std::data(t), std::size(t)});
		}
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_ARR) {
		if constexpr (looks_like_arr_v<T>) {
			add_arr(prefix, std::size(t));
			for (const auto& x : t)
				add_internal<compact::MP_END, false, void>(CStr<>(), x);
		} else if constexpr (is_tuple_v<T>) {
			add_arr(prefix, std::tuple_size_v<T>);
			std::apply([this](const auto& ...x) {
				(..., this->template add_internal<compact::MP_END, false, void>(CStr<>(), x));
			}, t);
		} else {
			static_assert(always_false_v<T>,
				      "Wrong thing was passed as array");
		}
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (TYPE == compact::MP_MAP) {
		if constexpr (looks_like_map_v<T>) {
			add_map(prefix, std::size(t));
			for (const auto& x : t) {
				add(x.first);
				add(x.second);
			}
		} else if constexpr (looks_like_arr_v<T>) {
			assert(std::size(t) % 2 == 0);
			add_map(prefix, std::size(t) / 2);
			for (const auto& x : t)
				add(x);
		} else if constexpr (is_tuple_v<T>) {
			static_assert(std::tuple_size_v<T> % 2 == 0,
				      "Map expects even number of elements");
			add_map(prefix, std::tuple_size_v<T> / 2);
			std::apply([this](const auto& ...x) {
				(..., this->template add_internal<compact::MP_END, false, void>(CStr<>(), x));
			}, t);
		} else {
			static_assert(always_false_v<T>,
				      "Wrong thing was passed as map");
		}
		add_internal<compact::MP_END, false, void>(CStr<>{}, more...);
	} else if constexpr (is_raw_v<T>) {
		static_assert(always_false_v<T>, "Not implemented!");
	} else if constexpr (is_reserve_v<T>) {
		static_assert(always_false_v<T>, "Not implemented!");
	} else if constexpr (is_ext_v<T>) {
		static_assert(always_false_v<T>, "Not implemented!");
	} else if constexpr (looks_like_str_v<T>) {
		add_internal<compact::MP_STR, FIXED_SET, FIXED_TYPE>(prefix, t, more...);
	} else if constexpr (is_c_str_v<T>) {
		add_internal<compact::MP_STR, FIXED_SET, FIXED_TYPE>(prefix, t, more...);
	} else if constexpr (looks_like_map_v<T>) {
		add_internal<compact::MP_MAP, FIXED_SET, FIXED_TYPE>(prefix, t, more...);
	} else if constexpr (looks_like_arr_v<T>) {
		add_internal<compact::MP_ARR, FIXED_SET, FIXED_TYPE>(prefix, t, more...);
	} else if constexpr (is_tuple_v<T>) {
		add_internal<compact::MP_ARR, FIXED_SET, FIXED_TYPE>(prefix, t, more...);
	} else {
		static_assert(always_false_v<T>, "Unknown type!");
	}
}


} // namespace mpp {
