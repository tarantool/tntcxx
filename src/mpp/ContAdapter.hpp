#pragma once
/*
 * Copyright 2010-2024 Tarantool AUTHORS: please see AUTHORS file.
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
#include <cstring>
#include <utility>

#include "../Utils/CStr.hpp"
#include "../Utils/Traits.hpp"

namespace mpp {

namespace encode_details {

/** Common data+size pair that is used for writing of variable-length data. */
struct WData {
	const char *data;
	size_t size;
};

/** Random struct; used to check whether container has template write method. */
struct TestWriteStruct {
	uint32_t a;
	uint16_t b;
};

/** Test that container if Buffer-like: has several needed write methods. */
template <class CONT, class _ = void>
struct is_write_callable_h : std::false_type {};

template <class CONT>
struct is_write_callable_h<CONT,
	std::void_t<decltype(std::declval<CONT>().write(uint8_t{})),
		    decltype(std::declval<CONT>().write(uint64_t{})),
		    decltype(std::declval<CONT>().write(TestWriteStruct{})),
		    decltype(std::declval<CONT>().write({(const char *)0, 1})),
		    decltype(std::declval<CONT>().write(tnt::CStr<'a', 'b'>{}))
		   >> : std::true_type {};

template <class CONT>
constexpr bool is_write_callable_v = is_write_callable_h<CONT>::value;

template <class CONT>
class BufferWriter {
public:
	explicit BufferWriter(CONT& cont_) : cont{cont_} {}
	void write(WData data) { cont.write({data.data, data.size}); }
	template <char... C>
	void write(tnt::CStr<C...> str)
	{
		cont.write(std::move(str));
	}
	template <class T>
	void write(T&& t)
	{
		cont.write(std::forward<T>(t));
	}

private:
	CONT& cont;
};

template <class CONT>
class StdContWriter {
public:
	static_assert(sizeof(*std::declval<CONT>().data()) == 1);
	explicit StdContWriter(CONT& cont_) : cont{cont_} {}
	void write(WData data)
	{
		size_t old_size = std::size(cont);
		cont.resize(old_size + data.size);
		std::memcpy(std::data(cont) + old_size, data.data, data.size);
	}
	template <char... C>
	void write(tnt::CStr<C...> data)
	{
		size_t old_size = std::size(cont);
		cont.resize(old_size + data.size);
		std::memcpy(std::data(cont) + old_size, data.data, data.size);
	}

	template <class T>
	void write(T&& t)
	{
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);
		size_t old_size = std::size(cont);
		cont.resize(old_size + sizeof(T));
		std::memcpy(std::data(cont) + old_size, &t, sizeof(T));
	}

private:
	CONT& cont;
};

template <class C>
class PtrWriter {
public:
	static_assert(sizeof(C) == 1);
	static_assert(!std::is_const_v<C>);
	explicit PtrWriter(C *& ptr_) : ptr{ptr_} {}
	void write(WData data)
	{
		std::memcpy(ptr, data.data, data.size);
		ptr += data.size;
	}
	template <char... D>
	void write(tnt::CStr<D...> data)
	{
		std::memcpy(ptr, data.data, data.size);
		ptr += data.size;
	}

	template <class T>
	void write(T&& t)
	{
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);
		std::memcpy(ptr, &t, sizeof(t));
		ptr += sizeof(t);
	}

private:
	C *& ptr;
};

template <class CONT>
auto
wr(CONT& cont)
{
	if constexpr (is_write_callable_v<CONT>)
		return BufferWriter<CONT>{cont};
	else if constexpr (tnt::is_resizable_v<CONT> && tnt::is_contiguous_v<CONT>)
		return StdContWriter<CONT>{cont};
	else if constexpr (std::is_pointer_v<CONT>)
		return PtrWriter<std::remove_pointer_t<CONT>>{cont};
	else
		static_assert(tnt::always_false_v<CONT>);
}

} // namespace encode_details

namespace decode_details {

/** Common data+size pair that is used for reading of variable-length data. */
struct RData {
	char *data;
	size_t size;
};

/** Struct that when used as read argument, means skipping of 'size' data. */
struct Skip {
	size_t size;
};

/** Random struct; used to check whether container has template read method. */
struct TestReadStruct {
	uint8_t a;
	uint64_t b;
};

/** Test that container if Buffer-like: has several needed read methods. */
template <class CONT, class _ = void>
struct is_read_callable_h : std::false_type {};

template <class CONT>
struct is_read_callable_h<CONT,
	std::void_t<decltype(std::declval<CONT>().read(*(uint8_t*)0)),
		    decltype(std::declval<CONT>().read(*(uint64_t*)0)),
		    decltype(std::declval<CONT>().read(*(TestReadStruct*)0)),
		    decltype(std::declval<CONT>().read({(char *)0, 1})),
		    decltype(std::declval<CONT>().template get<uint8_t>()),
		    decltype(std::declval<CONT>().read({1}))
		   >> : std::true_type {};

template <class CONT>
constexpr bool is_read_callable_v = is_read_callable_h<CONT>::value;

template <class CONT>
class BufferReader {
public:
	explicit BufferReader(CONT& cont_) : cont{cont_} {}
	void read(RData data) { cont.read({data.data, data.size}); }
	void read(Skip data) { cont.read({data.size}); }
	template <class T>
	void read(T&& t)
	{
		cont.read(std::forward<T>(t));
	}
	template <class T>
	T get()
	{
		return cont.template get<T>();
	}

private:
	CONT& cont;
};

template <class C>
class PtrReader {
public:
	static_assert(sizeof(C) == 1);
	explicit PtrReader(C *& ptr_) : ptr{ptr_} {}
	void read(RData data)
	{
		std::memcpy(data.data, ptr, data.size);
		ptr += data.size;
	}
	void read(Skip data) { ptr += data.size; }

	template <class T>
	void read(T& t)
	{
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);
		std::memcpy(&t, ptr, sizeof(t));
		ptr += sizeof(t);
	}
	template <class T>
	T get()
	{
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);
		T t;
		std::memcpy(&t, ptr, sizeof(t));
		return t;
	}

private:
	C *& ptr;
};

template <class CONT>
auto
rd(CONT& cont)
{
	if constexpr (is_read_callable_v<CONT>)
		return BufferReader<CONT>{cont};
	else if constexpr (std::is_pointer_v<CONT>)
		return PtrReader<std::remove_pointer_t<CONT>>{cont};
	else
		static_assert(tnt::always_false_v<CONT>);
}

} // namespace decode_details

} // namespace mpp
