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

#include <cstddef>
#include <type_traits>
#include <utility>

namespace tnt {

template <size_t MAX_OBJECT_SIZE>
struct ObjHolder {
	void (*destroy_f)(ObjHolder *) = nullptr;
	char data[MAX_OBJECT_SIZE];

	ObjHolder() = default;
	ObjHolder(const ObjHolder&) = delete;
	ObjHolder& operator=(const ObjHolder&) = delete;
	~ObjHolder() noexcept { destroy(); }

	template <class T, class... ARGS>
	void create(ARGS&&... args)
	{
		static_assert(sizeof(T) <= sizeof(data));
		T* ptr = new (data) T(std::forward<ARGS>(args)...);
		assert(ptr == reinterpret_cast<T*>(data)); (void)ptr;
		if constexpr (std::is_trivially_destructible_v<T>)
			destroy_f = nullptr;
		else
			destroy_f = [](ObjHolder *s) { s->get<T>().~T(); };
	}
	void destroy()
	{
		if (destroy_f != nullptr)
			destroy_f(this);
		destroy_f = nullptr;
	}
	template <class T>
	T &get()
	{
		return *reinterpret_cast<T*>(data);
	}
};

} // namespace tnt {
